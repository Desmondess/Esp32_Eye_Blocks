#include "esp_camera.h"
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"
//#define BLOCK_SIZE 30 //numero de frames salvos de uma vez
#include "block_handler.h"


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
    unsigned long frameTimer[BLOCK_SIZE]
    ) 
{
    //valores genéricos
    BlockHeader block;
    block.magic = 0;
    block.version = 1;
    block.frameCount = BLOCK_SIZE;
    block.duration = frameTimer[BLOCK_SIZE - 1] - frameTimer[0];
    block.prevHash = 0; // por enquanto inútil, não utilizado.

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

void decodeBlock(File file, String destiny)
{
    //uint8_t* frameBuffer[BLOCK_SIZE];
    //size_t frameSize[BLOCK_SIZE];
    //unsigned long frameTimer[BLOCK_SIZE];
    if(!file)
    {
        Serial.printf("File invalido");
        return;
    }

    //valores genéricos
    BlockHeader block;

    file.read((uint8_t*)&block,sizeof(block));
    uint32_t count = block.frameCount; 

    SD_MMC.mkdir(destiny);

    for(int i = 0; i < count; i++)
    {
        FrameHeader fh;

        file.read((uint8_t*)&fh,sizeof(fh));

        uint8_t *img = (uint8_t *)(ps_malloc(fh.size));

        if(img) 
        {
            file.read(img,fh.size);
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

}