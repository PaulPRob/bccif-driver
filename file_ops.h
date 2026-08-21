//////////////////////////////////////////////////////////////////////////////////////
//                         Function prototypes for file_ops.c                       //
//////////////////////////////////////////////////////////////////////////////////////

#ifndef FILE_OPS_H
#define FILE_OPS_H

/* File Read & write functions */
ssize_t read(struct file* , char*, size_t , loff_t*); 
ssize_t write(struct file* , const char*, size_t , loff_t*); 

/* File IO handling */
int ioctl(struct inode *, struct file *, unsigned int , unsigned long );
int open(struct inode *, struct file *);
int release(struct inode *, struct file *);

/* Private helper functions */
void clearallflags(bccif_info_struct *);
int checkwbflag(bccif_info_struct *);
int checkwbtimeoutflags(bccif_info_struct *);

#endif /* #ifndef FILE_OPS_H */
