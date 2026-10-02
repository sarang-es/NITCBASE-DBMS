#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];


// Find the buffer slot containing a particular disk block
int StaticBuffer::getBufferNum(int blockNum) {

    // Check whether the block number is valid.
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    // Search through all buffer slots.
    for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

        // Check whether this buffer is occupied
        // and contains the requested disk block.
        if (!metainfo[bufferIndex].free &&
            metainfo[bufferIndex].blockNum == blockNum) {

            return bufferIndex;
        }
    }

    // The requested block is not currently in the buffer.
    return E_BLOCKNOTINBUFFER;
}

StaticBuffer::StaticBuffer() {

    for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}


// Write back all modified blocks on system exit
StaticBuffer::~StaticBuffer() {

    for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

        // Check whether the buffer slot is occupied
        // and the block has been modified

        if (metainfo[bufferIndex].free == false && metainfo[bufferIndex].dirty == true) {

            // Write the modified block back to disk
            Disk::writeBlock(blocks[bufferIndex],metainfo[bufferIndex].blockNum);
        }
    }
}

int StaticBuffer::getFreeBuffer(int blockNum) {

    // Check whether block number is valid
    if (blockNum <= 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    /*
     * Increase timestamp of all occupied buffers.
     * Larger timestamp = older buffer
     * Smaller timestamp = recently used buffer
     */
    for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

        if (metainfo[bufferIndex].free == false) {
            metainfo[bufferIndex].timeStamp++;
        }
    }


    // Buffer number that will be returned
    int bufferNum = -1;


    /*
     * First look for a free buffer.
     */
    for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

        if (metainfo[bufferIndex].free == true) {
            bufferNum = bufferIndex;
            break;
        }
    }


    /*
     * If no free buffer is available,
     * find the buffer with the largest timestamp.
     */
    if (bufferNum == -1) {

        int maxTimeStamp = -1;

        for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

            if (metainfo[bufferIndex].timeStamp > maxTimeStamp) {

                maxTimeStamp = metainfo[bufferIndex].timeStamp;

                bufferNum = bufferIndex;
            }
        }


        /*
         * The selected buffer is the LRU buffer.
         * If it is dirty, write its contents back to disk
         * before reusing the buffer.
         */
        if (metainfo[bufferNum].dirty == true) {

            Disk::writeBlock(blocks[bufferNum],metainfo[bufferNum].blockNum);
        }
    }


    // Update metadata for the selected buffer.
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;


    return bufferNum;
}

int StaticBuffer::setDirtyBit(int blockNum) {

    // Find the buffer containing this disk block
    int bufferNum = getBufferNum(blockNum);

    // Block is not present in the buffer
    if (bufferNum == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }

    // Block number is out of bounds
    if (bufferNum == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }

    // Mark the buffer block as dirty
    metainfo[bufferNum].dirty = true;

    return SUCCESS;
}