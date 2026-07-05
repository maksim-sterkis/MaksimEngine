#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <filesystem>
#include <iostream>
#include <ktx.h>
#include <string>

namespace fs = std::filesystem;

bool convert_to_ktx2(const std::string &inputPath,
                     const std::string &outputPath) {
  int width, height, channels;
  stbi_uc *pixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 4);
  if (!pixels) {
    std::cerr << "Failed to load " << inputPath << "\n";
    return false;
  }

  ktxTexture2 *texture;
  ktxTextureCreateInfo createInfo;
  createInfo.vkFormat = 43; // VK_FORMAT_R8G8B8A8_SRGB
  createInfo.baseWidth = width;
  createInfo.baseHeight = height;
  createInfo.baseDepth = 1;
  createInfo.numDimensions = 2;
  createInfo.numLevels = 1;
  createInfo.numLayers = 1;
  createInfo.numFaces = 1;
  createInfo.isArray = KTX_FALSE;
  createInfo.generateMipmaps = KTX_FALSE;

  if (ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE,
                         &texture) != KTX_SUCCESS) {
    stbi_image_free(pixels);
    return false;
  }

  ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, 0, pixels,
                                width * height * 4);
  stbi_image_free(pixels);

  ktxBasisParams params = {0};
  params.structSize = sizeof(params);
  params.uastc = KTX_TRUE; // Use UASTC (fast and high quality)
  params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;

  if (ktxTexture2_CompressBasisEx(texture, &params) != KTX_SUCCESS) {
    std::cerr << "Failed to compress " << inputPath << "\n";
    ktxTexture_Destroy(ktxTexture(texture));
    return false;
  }

  if (ktxTexture_WriteToNamedFile(ktxTexture(texture), outputPath.c_str()) !=
      KTX_SUCCESS) {
    std::cerr << "Failed to save " << outputPath << "\n";
    ktxTexture_Destroy(ktxTexture(texture));
    return false;
  }

  ktxTexture_Destroy(ktxTexture(texture));
  std::cout << "Successfully converted " << inputPath << " to " << outputPath
            << "\n";
  return true;
}

int main() {
  std::string inDir = "assets/textures";
  std::string outDir = "assets/textures/ktx2";

  if (!fs::exists(inDir)) {
    fs::create_directories(inDir);
  }
  if (!fs::exists(outDir)) {
    fs::create_directories(outDir);
  }

  for (const auto &entry : fs::directory_iterator(inDir)) {
    if (!entry.is_regular_file())
      continue;

    std::string ext = entry.path().extension().string();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
      std::string filename = entry.path().stem().string();
      std::string outFile = outDir + "/" + filename + ".ktx2";

      bool needsUpdate = true;
      if (fs::exists(outFile)) {
        auto inTime = fs::last_write_time(entry.path());
        auto outTime = fs::last_write_time(outFile);
        if (inTime <= outTime) {
          needsUpdate = false;
        }
      }

      if (needsUpdate) {
        std::cout << "Compiling texture: " << entry.path().string() << "...\n";
        convert_to_ktx2(entry.path().string(), outFile);
      }
    }
  }

  return 0;
}
