#pragma once
#include <IL/il.h>
#include <glad/glad.h>
#include <string>
#include "../../../Utils/Log.h"

inline unsigned int LoadTexture(const char* path) 
{
    static bool ilInitialized = false;
    if (!ilInitialized) 
    {
        ilInit();
        ilInitialized = true;
    }

    ILuint imageID;
    ilGenImages(1, &imageID);
    ilBindImage(imageID);

    if (!ilLoadImage(path)) 
    {
        LOG("Error cargando la textura: %s", path);
        ilDeleteImages(1, &imageID);
        return 0;
    }

    ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width = ilGetInteger(IL_IMAGE_WIDTH);
    int height = ilGetInteger(IL_IMAGE_HEIGHT);
    void* data = ilGetData();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    ilDeleteImages(1, &imageID);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}