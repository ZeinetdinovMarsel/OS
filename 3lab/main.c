#include <stdio.h>
#include <stdlib.h>

#define FRAME_SIZE 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 256

typedef struct
{
    int pageNumber;
    int frameNumber;
} TLBEntry;

char physicalMemory[NUM_FRAMES][FRAME_SIZE];
TLBEntry tlb[TLB_SIZE];
int pageTable[PAGE_TABLE_SIZE];
int freeFrames[NUM_FRAMES];
int tlbPointer = 0;
int tlbHitCount = 0;

int fifoPointer = 0;

void readFromBackingStore(int pageNumber)
{
    FILE *backingStore = fopen("BACKING_STORE.bin", "rb");
    fseek(backingStore, pageNumber * FRAME_SIZE, SEEK_SET);
    fread(physicalMemory[fifoPointer], sizeof(char), FRAME_SIZE, backingStore);
    fclose(backingStore);
}

void updateTlb(int pageNumber, int frameNumber)
{
    tlb[tlbPointer].pageNumber = pageNumber;
    tlb[tlbPointer].frameNumber = frameNumber;
    tlbPointer = (++tlbPointer) % TLB_SIZE;
}

int getFreeFrame()
{
    int freeFrame = -1;
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        if (freeFrames[i] == 0)
        {
            freeFrame = i;
            freeFrames[i] = 1;
            break;
        }
    }
    return freeFrame;
}

int translateAddress(int virtualAddress)
{
    int pageNumber = (virtualAddress >> 8) & 0xFF;
    int offset = virtualAddress & 0xFF;
    int frameNumber;

    for (int i = 0; i < TLB_SIZE; i++)
    {
        if (tlb[i].pageNumber == pageNumber)
        {
            frameNumber = tlb[i].frameNumber;
            tlbHitCount++;
            return frameNumber * FRAME_SIZE + offset;
        }
    }

    if (pageTable[pageNumber] == -1)
    {
        int frameNumber = getFreeFrame();
        if (frameNumber == -1)
        {
            frameNumber = fifoPointer;
        }
        pageTable[pageNumber] = frameNumber;
        readFromBackingStore(pageNumber);
        fifoPointer = (++fifoPointer) % NUM_FRAMES;
    }

    frameNumber = pageTable[pageNumber];
    updateTlb(pageNumber, frameNumber);

    return frameNumber * FRAME_SIZE + offset;
}

int main()
{
    FILE *addressesFile = fopen("addresses.txt", "r");
    FILE *outputFile = fopen("output.txt", "w+");

    if (addressesFile == NULL || outputFile == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    for (int i = 0; i < TLB_SIZE; i++)
    {
        tlb[i].pageNumber = -1;
        tlb[i].frameNumber = -1;
    }

    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        pageTable[i] = -1;
    }

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        freeFrames[i] = 0;
    }

    int total = 0;
    int virtualAddress;
    while (fscanf(addressesFile, "%d", &virtualAddress) != EOF)
    {
        int physicalAddress = translateAddress(virtualAddress);
        char value = physicalMemory[physicalAddress / FRAME_SIZE][physicalAddress % FRAME_SIZE] & 0xFF;
        fprintf(outputFile, "Virtual address: %d Physical address: %d Value: %d\n",
                virtualAddress, physicalAddress, value);
        total++;
    }

    double pageFault_rate = (double)fifoPointer / total;
    double tlbHit_rate = (double)tlbHitCount / total;

    printf("Page fault rate: %.2f%%\n", pageFault_rate * 100);
    printf("TLB hit rate: %.2f%%\n", tlbHit_rate * 100);

    fclose(addressesFile);
    fclose(outputFile);
    return 0;
}