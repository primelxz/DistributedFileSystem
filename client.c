#include <stdio.h>
#include "udp.h"
#include "mfs.c"


int main(int argc, char *argv[]) {
	
	int rc = MFS_Init("localhost", 1217);
	
  return rc;
}
