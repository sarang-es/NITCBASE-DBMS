#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {

  // initialise all blocks as free
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
  }
}

/*
At this stage, we are not writing back from the buffer to the disk since we are
not modifying the buffer. So, we will define an empty destructor for now. In
subsequent stages, we will implement the write-back functionality here.
*/
StaticBuffer::~StaticBuffer() {}

// Find a free buffer slot
int StaticBuffer::getFreeBuffer(int blockNum) {

    // Check whether the block number is valid.
    if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    int allocatedBuffer = -1;

    // Search for the first free buffer slot.
    for (int bufferIndex = 0;bufferIndex < BUFFER_CAPACITY;bufferIndex++) {

        if (metainfo[bufferIndex].free) {

            allocatedBuffer = bufferIndex;
            break;
        }
    }

    // No free buffer was found.
    if (allocatedBuffer == -1) {
        return E_OUTOFBOUND;
    }

    // Mark this buffer as occupied.
    metainfo[allocatedBuffer].free = false;

    // Store which disk block will be kept in this buffer.
    metainfo[allocatedBuffer].blockNum = blockNum;

    // Return the buffer slot number.
    return allocatedBuffer;
}

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


