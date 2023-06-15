#define INODE_LIMIT (4096)
#define IMAP_ENTRIES (16)
#define DIR_ENTRIES (128)
#define INODE_PTRS (14)

enum REQUEST {
  INIT,
  LOOKUP,
  STAT,
  WRITE,
  READ,
  CREAT,
  UNLINK,
  RESPONSE,
  SHUTDOWN
};

// checkpoint region
typedef struct __MFS_CR_t
{
	int end;
	int imap[INODE_LIMIT / IMAP_ENTRIES];
} MFS_CR_t;

typedef struct __MFS_Inode_t
{
	int size;
	int type;
	int ptrs[INODE_PTRS];
} MFS_Inode_t;

// one imap scratch
typedef struct __MFS_Imap_t
{
	int inode_addr[IMAP_ENTRIES];
} MFS_Imap_t;

typedef struct __MFS_Dir_t
{
	MFS_DirEnt_t entries[DIR_ENTRIES];
} MFS_Dir_t;

// network message
typedef struct __MFS_Msg_t
{
  enum REQUEST req;
	char name[28];
	char buffer[MFS_BLOCK_SIZE];

	int inum;
	int block;
	MFS_Stat_t stat;
} MFS_MSG_t;