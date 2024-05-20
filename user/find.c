#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find_rec(char * dirpath, char * filename) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  char * path = dirpath;
  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    if (strcmp(filename, path + strlen(path) - strlen(filename)) == 0) {
      printf("%s\n", path);

    }
  } else if (st.type == T_DIR) {
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if(de.inum == 0)
        continue;
      
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
          continue;

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find_rec(buf, filename);
    }
  }
  close(fd);
}

int main(int argc, char *argv[])  {
  find_rec(argv[1], argv[2]);
  exit(0);
}

