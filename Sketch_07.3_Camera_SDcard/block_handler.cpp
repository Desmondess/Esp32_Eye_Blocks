#include "esp_camera.h"
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"
//#define BLOCK_SIZE 30 //numero de frames salvos de uma vez
#include "block_handler.h"
#include "mbedtls/sha256.h"
#include <algorithm>


//struct dos Blocos
//Vai salvar os blocos em binário puro em 1 único arquivo por bloco.

/*struct BlockHeader
{
    uint32_t magic; // serve pra dizer o tipo de arquivo, todo exemplo que eu peguei tinha isso, só copiei.
    uint32_t version;
    uint32_t frameCount;
    uint32_t duration;
    uint32_t prevHash; // por enquanto inútil, não utilizado.
};
struct FrameHeader
{
    uint32_t timestamp;
    uint32_t size;
};

/*struct Frame
{
    uint8_t frameData[600000];
    size_t frameSize;
    unsigned long timeStamp;
    //unsigned long blockTimer = 0;
};

struct Block
{
    BlockHeader header;
    Frame frames[BLOCK_SIZE];
};*/


void writeBlock(String destiny,
    uint8_t* frameBuffer[BLOCK_SIZE],
    size_t frameSize[BLOCK_SIZE],
    unsigned long frameTimer[BLOCK_SIZE],
    String prevPath
    ) 
{
    //valores genéricos
    BlockHeader block;
    block.magic = 0;
    block.version = 1;
    block.frameCount = BLOCK_SIZE;
    block.duration = frameTimer[BLOCK_SIZE - 1] - frameTimer[0];
    //block.prevHash = {0}; // por enquanto inútil, não utilizado.
    File prev = SD_MMC.open(prevPath, FILE_READ);
    if(prevPath)
        CalcHash(prev, block.prevHash);
    else
        memset(block.prevHash, 0, sizeof(block.prevHash));

    prev.close();

    File file = SD_MMC.open(destiny, FILE_WRITE);
    if(!file)
    {
        Serial.println("Erro abrindo arquivo");
        return;
    }
    file.write((uint8_t*)&block, sizeof(block));

    FrameHeader fh;

    for(int i = 0; i < BLOCK_SIZE; i++)
    {
        fh.timestamp = frameTimer[i];
        fh.size = frameSize[i];
        file.write((uint8_t*)&fh, sizeof(fh));
        file.write(frameBuffer[i], frameSize[i]);
    }

    file.close();


}

void decodeBlock(String target, String previous, String destiny)
{

    File curr = SD_MMC.open(target, FILE_READ);
    File prev = SD_MMC.open(previous, FILE_READ);
    //uint8_t* frameBuffer[BLOCK_SIZE];
    //size_t frameSize[BLOCK_SIZE];
    //unsigned long frameTimer[BLOCK_SIZE];
    if(!curr)
    {
        Serial.printf("File invalido");
        return;
    }

    BlockHeader block;
    curr.read((uint8_t*)&block,sizeof(block));

    //Testa se é o primeiro bloco (hash apenas zeros)
    uint8_t zero[32] = {0};
    bool first = std::equal(zero, zero + 32, block.prevHash);
    if(!prev){
        if(!first)
            {
                Serial.printf("Chain invalida");
                curr.close();  
                return;
            }
    }
    

    //Passa o bloco anterior pelo hash.
    uint8_t prevHash[32] = {0};
    
    if(!first)
        CalcHash(prev, prevHash);
    
    if(prev)
        prev.close();

    bool equal = std::equal(prevHash, prevHash + 32, block.prevHash);

    if(!equal)
    {
        Serial.printf("Chain inválida.");
        curr.close();
        return;
    }

    uint32_t count = block.frameCount;



    //checa se o prevReference é tudo 0
    //bool first = std::all_of(prevReference,
    //prevReference + 32,
    //[](uint8_t b) { return b == 0; });

    //if(!first){

    //}

    SD_MMC.mkdir(destiny);

    for(int i = 0; i < count; i++)
    {
        FrameHeader fh;

        curr.read((uint8_t*)&fh,sizeof(fh));

        uint8_t *img = (uint8_t *)(ps_malloc(fh.size));

        if(img) 
        {
            curr.read(img,fh.size);
            // img contém um JPEG completo (é pra conter).

            //frame.write(img, sizeof(img));

            String framePath = destiny + "/frame" + String(i) + ".jpg";
            File frame = SD_MMC.open(framePath, FILE_WRITE);
            frame.write(img, fh.size);

            frame.close();
            free(img);

            Serial.printf("Salvou %s \n", framePath);
        }
        else
        {
            Serial.printf("Erro ao alocar memoriaa");
            return;
            //free(img);
        }
    }
        String timePath = destiny + "/timer.txt";
        File Timer = SD_MMC.open(timePath, FILE_WRITE);
        Timer.println(String(block.duration));
        Timer.close();
        
        curr.close();
}

bool CalcHash(File file, uint8_t outputHash[32]) {
    if (!file)
    {
        Serial.println("Arquivo invalido");
        return false;
    }

    file.seek(0);

    //inicializa o hash
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);

    uint8_t buffer[512];

    while (file.available()) //descobri hoje que file.available existe
    {
        size_t bytesRead = file.read(buffer, sizeof(buffer));

        if (bytesRead == 0)
            break;

        mbedtls_sha256_update(&ctx,buffer,bytesRead);
    }

    //coloca o hash no mesmo ponteiro que recebeu
    mbedtls_sha256_finish(&ctx,outputHash);

    mbedtls_sha256_free(&ctx);

    file.seek(0);

    return true;
}