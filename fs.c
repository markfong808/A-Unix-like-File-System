// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}

// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void *buf)
{
  if (numb < 0)
    FATAL(ENEGNUMB);

  // Get inum and current cursor position from OFT
  i32 inum = bfsFdToInum(fd);
  i32 cursor = bfsTell(fd);
  // printf("Cursor location: %d\n", cursor);

  // Get the size of the file
  i32 fileSize = bfsGetSize(inum);
  //printf("Filesize: %d\n", fileSize);

  // Buffer to store data read from disk
  i8 readBuf[BYTESPERBLOCK];
  i32 bytesRead = 0;

  while (bytesRead < numb)
  {
    if (cursor + bytesRead >= fileSize)
      break; // Stop reading if end of file is reached

    // Calculate FBN for current cursor position
    i32 currentFbn = (cursor + bytesRead) / BYTESPERBLOCK;

    // Translate FBN to DBN
    i32 dbn = bfsFbnToDbn(inum, currentFbn);
    if (dbn == ENODBN)
      break;

    // Read the block from disk
    bioRead(dbn, readBuf);

    // Determine how many bytes to copy from the block
    i32 blockOffset = (cursor + bytesRead) % BYTESPERBLOCK;
    i32 remaining = (numb - bytesRead) < (fileSize - (cursor + bytesRead)) ? (numb - bytesRead) : (fileSize - (cursor + bytesRead));
    i32 bytesToRead = (BYTESPERBLOCK - blockOffset) < remaining ? (BYTESPERBLOCK - blockOffset) : remaining;

    memcpy((i8 *)buf + bytesRead, readBuf + blockOffset, bytesToRead);
    bytesRead += bytesToRead;
  }
  //  Update cursor position after reading
  fsSeek(fd, bytesRead, SEEK_CUR);

  // Return the number of bytes actually read
  return bytesRead;
}

// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {

  // ++++++++++++++++++++++++
  // Insert your code here
  // ++++++++++++++++++++++++
  if (numb < 0)
    FATAL(ENEGNUMB);

  // Get inum and current cursor position from OFT
  i32 inum = bfsFdToInum(fd);
  i32 cursor = bfsTell(fd);

  i32 bytesWritten = 0;

  while (bytesWritten < numb)
  {
    // Calculate FBN for current cursor position
    i32 fbn = (cursor + bytesWritten) / BYTESPERBLOCK;

    // Translate FBN to DBN
    i32 dbn = bfsFbnToDbn(inum, fbn);
    if (dbn == ENODBN)
      dbn = bfsAllocBlock(inum, fbn);

    i32 blockOffset = (cursor + bytesWritten) % BYTESPERBLOCK;
    i32 bytesInBlock = (BYTESPERBLOCK - blockOffset) < (numb - bytesWritten) ? (BYTESPERBLOCK - blockOffset) : (numb - bytesWritten);

    // Buffer to store data to be written to disk
    i8 writeBuf[BYTESPERBLOCK];

    // If not writing a full block, read the existing block first
    if (bytesInBlock < BYTESPERBLOCK)
      bioRead(dbn, writeBuf);

    // Copy data from the user buffer to the write buffer
    memcpy(writeBuf + blockOffset, (i8 *)buf + bytesWritten, bytesInBlock);

    // Write the block back to disk
    bioWrite(dbn, writeBuf);

    // Update the number of bytes written and the cursor position
    bytesWritten += bytesInBlock;
  }

  // Update cursor position after writting
  fsSeek(fd, bytesWritten, SEEK_CUR);

  // Update the file size if necessary
  i32 newSize = cursor + bytesWritten;
  if (newSize > bfsGetSize(inum))
    bfsSetSize(inum, newSize);

  return 0;
  // FATAL(ENYI);                                  // Not Yet Implemented!
  // return 0;
}
