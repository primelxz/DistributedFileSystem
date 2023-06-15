#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "mfs.h"
#include "udp.h"
#include "struct.h"

char *s_host = NULL;
int s_port = -1;

int name_checker(char* name){
	if (name == NULL){
		return 1;
	}
	if (strlen(name) > 28)
  {
    return 1;
  }
	return 0;
}

int Sd_Msg(MFS_MSG_t *send, MFS_MSG_t *receive, char *host, int port)
{
  int sd = UDP_Open(0);
  struct sockaddr_in addrSnd, addrRcv;
  int rc = UDP_FillSockAddr(&addrSnd, host, port);

      fd_set fdset;
  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 0;

  int times = 5;
  do
  {
    FD_ZERO(&fdset);
    FD_SET(sd, &fdset);
    UDP_Write(sd, &addrSnd, (char *)send, sizeof(MFS_MSG_t));
    if (select(sd + 1, &fdset, NULL, NULL, &tv))
    {
      rc = UDP_Read(sd, &addrRcv, (char *)receive, sizeof(MFS_MSG_t));
      if (rc > 0)
      {
        UDP_Close(sd);
        return 0;
      }
    }
    else
    {
      times--;
    }
  } while (1);
}

int MFS_Init(char *hostname, int port)
{
  s_host = hostname;
  s_port = port;
  return 0;
}

int MFS_Lookup(int pinum, char *name)
{
	if(name_checker(name)){
		return -1;
	}

  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.inum = pinum;
  strcpy(msg_sd.name, name);
  msg_sd.req = LOOKUP;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  return msg_rc.inum;
}

int MFS_Stat(int inum, MFS_Stat_t *m)
{
  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.inum = inum;
  msg_sd.req = STAT;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  m->type = msg_rc.stat.type;
  m->size = msg_rc.stat.size;
  return 0;
}

int MFS_Write(int inum, char *buffer, int block)
{
  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.inum = inum;
  msg_sd.block = block;
  for (int i = 0; i < MFS_BLOCK_SIZE; i++)
  {
    msg_sd.buffer[i] = buffer[i];
  }
  msg_sd.req = WRITE;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  return msg_rc.inum;
}

int MFS_Read(int inum, char *buffer, int block)
{
  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.inum = inum;
  msg_sd.block = block;
  msg_sd.req = READ;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  if (msg_rc.inum >= 0)
  {
    for (int i = 0; i < MFS_BLOCK_SIZE; i++)
    {
      buffer[i] = msg_rc.buffer[i];
    }
  }
  return msg_rc.inum;
}

int MFS_Creat(int pinum, int type, char *name)
{
  if(name_checker(name)){
		return -1;
	}

  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.inum = pinum;
  msg_sd.stat.type = type;
  strcpy(msg_sd.name, name);
  msg_sd.req = CREAT;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  return msg_rc.inum;
}

int MFS_Unlink(int pinum, char *name)
{
  if(name_checker(name)){
		return -1;
	}

  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.inum = pinum;
  strcpy(msg_sd.name, name);
  msg_sd.req = UNLINK;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  return msg_rc.inum;
}

int MFS_Shutdown()
{
  MFS_MSG_t msg_sd, msg_rc;
  msg_sd.req = SHUTDOWN;

  if (Sd_Msg(&msg_sd, &msg_rc, s_host, s_port) < 0)
  {
    return -1;
  }
  return 0;
}
