/* Filesystem - sample usage.
 *
 * The testing of the fs driver requires a backend (simulating the underlying disk block device).
 * Next, tests of your fs implemetnation are needed. To help you with the implementation,
 * a sample backend is implemented in this file. It provides a quick-and-dirty
 * implementation of the underlying disk (simulated in a file) and a few CFileSystem calls.
 *
 * The implementation in the real testing environment is different. The sample below is a
 * minimalistic disk backend which matches the required interface.
 *
 * You will have to add some FS testing. There are some CFileSystem methods called from within
 * main(), however, the tests are incomplete. Once again, this is only a starting point.
 */


#define DISK_SECTORS 8192
static FILE  * g_Fp = NULL;

//-------------------------------------------------------------------------------------------------
/** Sample sector reading function. The function will be called by your fs driver implementation.
 * Notice, the function is not called directly. Instead, the function will be invoked indirectly
 * through function pointer in the TBlkDev structure.
 */
static size_t      diskRead                                ( size_t            sectorNr,
                                                             void            * data,
                                                             size_t            sectorCnt )
{
  if ( g_Fp == NULL 
       || sectorNr + sectorCnt > DISK_SECTORS ) 
    return 0;
  fseek ( g_Fp, sectorNr * SECTOR_SIZE, SEEK_SET );
  return fread ( data, SECTOR_SIZE, sectorCnt, g_Fp );
}
//-------------------------------------------------------------------------------------------------
/** Sample sector writing function. Similar to diskRead
 */
static size_t      diskWrite                               ( size_t            sectorNr,
                                                             const void      * data,
                                                             size_t            sectorCnt )
{
  if ( g_Fp == NULL 
       || sectorNr + sectorCnt > DISK_SECTORS ) 
    return 0;
  fseek ( g_Fp, sectorNr * SECTOR_SIZE, SEEK_SET );
  return fwrite ( data, SECTOR_SIZE, sectorCnt, g_Fp );
}
//-------------------------------------------------------------------------------------------------
/** A function which creates the file needed for the sector reading/writing functions above.
 * This function is only needed for the particular implementation above. It could be understand as
 * "buying a new disk".
 */
static TBlkDev     createDisk                              ( void )
{
  char       buffer[SECTOR_SIZE];
 
  memset    ( buffer, 0, sizeof ( buffer ) );
  g_Fp = fopen ( "/tmp/disk_content", "w+b" );
  if ( ! g_Fp ) 
    throw "Error creating backed block device";
 
  for ( int i = 0; i < DISK_SECTORS; i ++ )
    if ( fwrite ( buffer, sizeof ( buffer ), 1, g_Fp ) != 1 )
      throw "Error creating backed block device";
  
  TBlkDev res;
  res . m_Sectors = DISK_SECTORS;
  res . m_Read    = diskRead;
  res . m_Write   = diskWrite;
  return res;
}
//-------------------------------------------------------------------------------------------------
/** A function which opens the files needed for the sector reading/writing functions above.
 * This function is only needed for the particular implementation above. It could be understand as
 * "turning the computer on".
 */
static TBlkDev     openDisk                                ( void )
{
  g_Fp = fopen ( "/tmp/disk_content", "r+b" );
  if ( ! g_Fp ) 
    throw "Error opening backend block device";
  fseek ( g_Fp, 0, SEEK_END );
  if ( ftell ( g_Fp ) != DISK_SECTORS * SECTOR_SIZE )
  {
    fclose ( g_Fp );
    g_Fp = NULL;
    throw "Error opening backend block device";
  }

  TBlkDev  res;
  res . m_Sectors = DISK_SECTORS;
  res . m_Read    = diskRead;
  res . m_Write   = diskWrite;
  return res;
}
//-------------------------------------------------------------------------------------------------
/** A function which releases resources allocated by openDisk/createDisk
 */
static void        doneDisk                                ( void )
{
  if ( g_Fp )
  {
    fclose ( g_Fp );
    g_Fp  = NULL;
  }
}
//-------------------------------------------------------------------------------------------------
static void        testMkFs                                ( void )
{
  /* Create the disk backend and format it using youe FsCreate call
   */
  
  assert ( CFileSystem::CreateFs ( createDisk () ) );
  doneDisk ();
}
//-------------------------------------------------------------------------------------------------
static void        testWrite                               ( void )
{
  char      buffer [100];
  TFile     info;
  /**
   * mount the peviously created fs
   */
  CFileSystem * fs = CFileSystem::Mount ( openDisk () );
  assert ( fs );

  /* your fs shall be ready. Try to create a file named "hello" inside your filesystem:
   */

  int fd = fs -> OpenFile ( "hello", true );

  for ( int i = 0; i < 100; i ++ )
    buffer[i] = i;

  assert ( fs -> WriteFile ( fd, buffer, 100 ) == 100 );
  assert ( fs -> CloseFile ( fd ) );

  /* test other fs operations here */
  

  /* umount the filesystem
   */
  assert ( fs -> Umount () );
  delete fs;
  
  /* ... and the underlying disk.
   */
  doneDisk ();
  
  
  /* The FS as well as the underlying disk simulation is stopped. It corresponds i.e. to the
   * restart of a real computer.
   *
   * after the restart, we will not create the disk,  nor create FS (we do not
   * want to destroy the content). Instead, we will only open/mount the devices:
   */

  
  fs = fs -> Mount ( openDisk () );
  assert ( fs );
  /* some I/O, tests, list the files
   */

  for ( bool found = fs -> FindFirst ( info ); found; found = fs -> FindNext ( info )   )
    printf ( "%-30s %6zd\n", info . m_FileName, info . m_FileSize );
     
  assert ( fs -> Umount () );
  delete fs;
  doneDisk ();
}
//-------------------------------------------------------------------------------------------------
int main ( void )
{
  testMkFs ();
  testWrite ();
  return 0;
}
