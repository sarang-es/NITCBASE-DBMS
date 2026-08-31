#include "Disk_Class/Disk.h"
#include "Cache/OpenRelTable.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  return FrontendInterface::handleFrontend(argc, argv);
}