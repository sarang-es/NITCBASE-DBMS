#include "Disk_Class/Disk.h"
#include "Buffer/BlockBuffer.h"
#include <iostream>
using namespace std;

int main() {

    Disk disk;

    unsigned char buffer[BLOCK_SIZE];

    int status = Disk::readBlock(buffer, 0);

    if(status != SUCCESS) {
        cout << "Read failed\n";
        return 1;
    }

    for(int i = 0; i < BLOCK_SIZE; i++) {

        cout << (int)buffer[i] << " ";

        if((i + 1) % 32 == 0)
            cout << endl;
    }

    return 0;
}