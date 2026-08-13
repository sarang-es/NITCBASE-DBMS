#include "BlockBuffer.h"
#include "../Disk_Class/Disk.h"
#include <cstring>


// Constructor
BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}


// Calls parent class constructor
RecBuffer::RecBuffer(int blockNum)
    : BlockBuffer(blockNum) {
}


// Load the block header into *head
int BlockBuffer::getHeader(struct HeadInfo *head) {

    unsigned char buffer[BLOCK_SIZE];

    // Read the block into buffer
    int status = Disk::readBlock(buffer, this->blockNum);

    if (status != SUCCESS) {
        return status;
    }

    // Extract header fields
    memcpy(&head->blockType,  buffer + 0,  4);
    memcpy(&head->pblock,     buffer + 4,  4);
    memcpy(&head->lblock,     buffer + 8,  4);
    memcpy(&head->rblock,     buffer + 12, 4);
    memcpy(&head->numEntries, buffer + 16, 4);
    memcpy(&head->numAttrs,   buffer + 20, 4);
    memcpy(&head->numSlots,   buffer + 24, 4);
    memcpy(head->reserved,    buffer + 28, 4);

    return SUCCESS;
}


// Load record at slotNum into rec
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {

    struct HeadInfo head;

    // Get header information
    int status = this->getHeader(&head);

    if (status != SUCCESS) {
        return status;
    }

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char buffer[BLOCK_SIZE];

    // Read block into buffer
    status = Disk::readBlock(buffer, this->blockNum);

    if (status != SUCCESS) {
        return status;
    }

    // Calculate record size
    int recordSize = attrCount * ATTR_SIZE;

    // Find the beginning of the required record
    unsigned char *slotPointer =
        buffer +
        HEADER_SIZE +
        slotCount +
        (recordSize * slotNum);

    // Copy record into rec
    memcpy(rec, slotPointer, recordSize);

    return SUCCESS;
}