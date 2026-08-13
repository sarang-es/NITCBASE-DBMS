#include "Disk_Class/Disk.h"
#include "Buffer/BlockBuffer.h"
#include <iostream>

int main() {

    Disk disk;

    unsigned char buffer[BLOCK_SIZE];

    int status = Disk::readBlock(buffer, 0);

    if(status == SUCCESS) {
        std::cout << "Block 0 read successfully\n";
    }
    else {
        std::cout << "Error reading block\n";
    }

    return 0;
}