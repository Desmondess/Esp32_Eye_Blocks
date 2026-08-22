#ifndef BLOCK_HANDLER_H
#define BLOCK_HANDLER_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "mbedtls/sha256.h"

#define BLOCK_SIZE 30 // número de frames salvos de uma vez


// Cabeçalho do bloco
struct BlockHeader
{
    uint32_t magic;       // identifica o tipo do arquivo
    uint32_t version;     // versão do formato
    uint32_t frameCount;  // quantidade de frames
    uint32_t duration;    // duração total do bloco
    uint8_t prevHash[32];    // reservado para uso futuro
};


// Cabeçalho de cada frame
struct FrameHeader
{
    uint32_t timestamp;   // tempo da captura
    uint32_t size;        // tamanho do JPEG em bytes
};


// Salva um bloco binário contendo vários frames
void writeBlock(
    String destiny,
    uint8_t* frameBuffer[BLOCK_SIZE],
    size_t frameSize[BLOCK_SIZE],
    unsigned long frameTimer[BLOCK_SIZE],
    String prevPath
);


// Extrai os frames JPEG de um bloco
void decodeBlock(
    String target,
    String Previous,
    String destiny
);
bool CalcHash(File file, uint8_t outputHash[32]);


#endif