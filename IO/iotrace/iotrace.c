#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define BUF_SIZE 1024

int main(void)
{
    int fd,size,len;
    int ret = 0;

    /*1. 普通io模式 */
    char * normal_write_buf = "normal io";
    len = strlen(normal_write_buf);
    char normal_read_buf[100] = {0};

    /* mark start */
    pid_t pid  = getpid();

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    /* 第一次wrirte，此时文件应该还不在page cache */
    if ((size = write( fd, normal_write_buf, len)) < 0){
        perror("write:");
        exit(1);
    }  

    /* 第二次wrirte，此时文件应该在page cache，因此路径会比上一次端 */
    if ((size = write( fd, normal_write_buf, len)) < 0){
        perror("write:");
        exit(1);
    }  
  
    lseek(fd, 0, SEEK_SET );
    
    if ((size = read( fd, normal_read_buf, len))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(normal_read_buf,normal_write_buf,len) != 0){
       perror("strncmp:");
       exit(1); 
    }

    /* 强制刷盘 */
    if(fsync(fd) < 0){
        perror("fysnc:");
        exit(1);
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    /*2. direct io模式*/
    
    char *direct_write_buf,*direct_read_buf;
    ret = posix_memalign((void **)&direct_write_buf, 512, BUF_SIZE);
    if (ret) {
        perror("posix_memalign:");
        exit(1);
    }

    ret = posix_memalign((void **)&direct_read_buf, 512, BUF_SIZE);
    if (ret) {
        perror("posix_memalign:");
        exit(1);
    }

    strcpy(direct_write_buf,"direct mode");
    len = strlen("direct mode");

    /* mark start */
    pid  = getpid();

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR | O_DIRECT,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    if ((size = write(fd, direct_write_buf, BUF_SIZE)) < 0){
        perror("write:");
        exit(1);
    }  

    /* direct模式只是说不经过page cache,但是数据要想持久化还必须用fsync */
    if(fsync(fd) < 0){
        perror("fysnc:");
        exit(1);
    }

    lseek(fd, 0, SEEK_SET );
    
    if ((size = read( fd, direct_read_buf, BUF_SIZE))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(direct_read_buf,direct_write_buf,len) != 0){
       perror("strncmp:");
       exit(1); 
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    /*3. 使用O_SYNC标志 */

    char * o_sync_write_buf = "o_sync io";
    len = strlen(o_sync_write_buf);
    char o_sync_read_buf[100] = {0};

    pid  = getpid();

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR | O_SYNC ,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    if ((size = write( fd, o_sync_write_buf, len)) < 0){
        perror("write:");
        exit(1);
    }  

    if ((size = write( fd, o_sync_write_buf, len)) < 0){
        perror("write:");
        exit(1);
    }  

    /* 使用了O_SYNC模式后再调用fsync会是什么行为 */
    if(fsync(fd) < 0){
        perror("fysnc:");
        exit(1);
    }

    lseek(fd, 0, SEEK_SET );
    
    if ((size = read( fd, o_sync_read_buf, len))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(o_sync_read_buf,o_sync_write_buf,len) != 0){
       perror("strncmp:");
       exit(1); 
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    /*4. 使用__O_DIRECT 和 O_SYNC 组合 */

    char *o_sync_direct_write_buf,*o_sync_direct_read_buf;
    ret = posix_memalign((void **)&o_sync_direct_write_buf, 512, BUF_SIZE);
    if (ret) {
        perror("posix_memalign:");
        exit(1);
    }

    ret = posix_memalign((void **)&o_sync_direct_read_buf, 512, BUF_SIZE);
    if (ret) {
        perror("posix_memalign:");
        exit(1);
    }

    strcpy(direct_write_buf,"o_sync o_direct mode");
    len = strlen("o_sync o_direct mode");

    pid  = getpid();

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR | O_DIRECT | O_SYNC ,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    if ((size = write( fd, o_sync_direct_write_buf, BUF_SIZE)) < 0){
        perror("write:");
        exit(1);
    }  

    if(fsync(fd) < 0){
        perror("fysnc:");
        exit(1);
    }

    lseek(fd, 0, SEEK_SET );
    
    if ((size = read( fd, o_sync_direct_read_buf, BUF_SIZE))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(o_sync_direct_read_buf,o_sync_direct_write_buf,len) != 0){
       perror("strncmp:");
       exit(1); 
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    /*5. mmap io use msync */

    char * mmap_write_buf = "mmap io";
    char mmap_read_buf[100];
    len = strlen(mmap_write_buf);
    
    pid  = getpid();
    char * file_map = NULL;

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR ,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    if((file_map = mmap(0, 1024, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0)) == (void *)-1){
        perror("mmap:");
        exit(1);  
    }

    if(fallocate(fd,0,0,BUF_SIZE) < 0){
       perror("fallocate:");
       exit(1);
    }

    memcpy(file_map,mmap_write_buf,strlen(mmap_write_buf));

    /* 此处不签msync就可以read，因为都在page cache中 */
    if ((size = read( fd, mmap_read_buf, strlen(mmap_write_buf)))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(mmap_read_buf,mmap_write_buf,strlen(mmap_write_buf)) != 0){
        perror("strncmp:");
        exit(1);
    }
    
    /* msync和fsync区别？ */
    if(msync(file_map,strlen(mmap_write_buf),MS_SYNC) < 0){
        perror("msync:");
        exit(1);
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    munmap(file_map,1024);

    /*6. mmap io use fsync */

    char * mmap_write_buf2 = "mmap io 2";
    char mmap_read_buf2[100];
    len = strlen(mmap_write_buf2);
    
    pid  = getpid();
    file_map = NULL;

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR ,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    if((file_map = mmap(0, 1024, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0)) == (void *)-1){
        perror("mmap:");
        exit(1);  
    }

    if(fallocate(fd,0,0,BUF_SIZE) < 0){
       perror("fallocate:");
       exit(1);
    }

    memcpy(file_map,mmap_write_buf2,strlen(mmap_write_buf2));

    /* 此处不签msync就可以read，因为都在page cache中 */
    if ((size = read( fd, mmap_read_buf2, strlen(mmap_write_buf2)))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(mmap_read_buf2,mmap_write_buf2,strlen(mmap_write_buf)) != 0){
        perror("strncmp:");
        exit(1);
    }
    
    /* msync和fsync区别？ */
    if(fsync(fd) < 0){
        perror("fsync:");
        exit(1);
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    munmap(file_map,1024);

    /*7. mmap io with O_DIRECT */
    char * mmap_direct_write_buf = "mmap direct mode";
    char * mmap_direct_read_buf;

    len = strlen(mmap_direct_write_buf);

    ret = posix_memalign((void **)&mmap_direct_read_buf, 512, BUF_SIZE);
    if (ret) {
        perror("posix_memalign:");
        exit(1);
    }

    pid  = getpid();
    file_map = NULL;

    if ((fd = open("hello.c", O_CREAT | O_TRUNC | O_RDWR | O_DIRECT,0666 ))<0) {
        perror("open:");
        exit(1);
    }  

    if((file_map = mmap(0, 1024, PROT_READ | PROT_WRITE, MAP_SHARED , fd, 0)) == (void *)-1){
        perror("mmap:");
        exit(1);  
    }

    if(fallocate(fd,0,0,BUF_SIZE) < 0){
       perror("fallocate:");
       exit(1);
    }

    memcpy(file_map,mmap_direct_write_buf,len);

    /* 由于使用了O_DIRECT，如果之前不msync，那么此时read会绕过page cahce，能读到正确数据？ */
    if ((size = read( fd, mmap_direct_read_buf, BUF_SIZE))<0) {
        perror("read:");
        exit(1);
    }  

    if(strncmp(mmap_direct_read_buf,mmap_direct_write_buf,len) != 0){
        perror("strncmp:");
        exit(1);
    }

    /* msync和fsync区别？ */
    if(msync(file_map,strlen(mmap_write_buf),MS_SYNC) < 0){
        perror("msync:");
        exit(1);
    }

    if ( close(fd) < 0 )    {
        perror("close:");
        exit(1);
    }  

    munmap(file_map,1024);

    return 0;
}

