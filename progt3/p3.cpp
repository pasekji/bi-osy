#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <functional>
#include <iostream>
using namespace std;


/* Filesystem size: min 8MiB, max 1GiB
 * Filename length: min 1B, max 28B
 * Sector size: 512B
 * Max open files: 8 at a time
 * At most one filesystem mounted at a time.
 * Max file size: < 1GiB
 * Max files in the filesystem: 128
 */

#define FILENAME_LEN_MAX    28
#define DIR_ENTRIES_MAX     128
#define OPEN_FILES_MAX      8
#define SECTOR_SIZE         512
#define DEVICE_SIZE_MAX     ( 1024 * 1024 * 1024 )
#define DEVICE_SIZE_MIN     ( 8 * 1024 * 1024 )

struct TFile
{
    char            m_FileName[FILENAME_LEN_MAX + 1];
    size_t          m_FileSize;
};

struct TBlkDev
{
    size_t           m_Sectors;
    function<size_t(size_t, void*, size_t)> m_Read;
    function<size_t(size_t, const void*, size_t)> m_Write;
};
#endif /* __PROGTEST__ */

#define ROUND_TO_MULTIPLE(a,b) (((a)+(b)-1)/(b)*(b))
#define SECTORS_IN_BLOCK    8
#define BLOCK_SIZE     ( SECTORS_IN_BLOCK * SECTOR_SIZE )
#define ADDRESES_IN_SECTOR (SECTOR_SIZE / sizeof(int32_t))

struct FileInfo
{
    int32_t fileSize;
    char fileName[(FILENAME_LEN_MAX + 1)];
#pragma warning( push )
#pragma warning( disable : 26495)
    char padding[ROUND_TO_MULTIPLE(FILENAME_LEN_MAX + 1, sizeof(int32_t)) - (FILENAME_LEN_MAX + 1)];
    int32_t blockAddresses[(SECTOR_SIZE - sizeof(fileSize) - sizeof(fileName) - sizeof(padding)) / sizeof(int32_t)];
    FileInfo(const char* fileName)
        : fileSize(0)
    {
        strncpy(this->fileName, fileName, FILENAME_LEN_MAX + 1);
    }
#pragma warning( pop ) 
    bool IsFileHere()
    {
        return fileSize >= 0;
    }
};

struct FileContinuation
{
    int32_t blockAddresses[ADDRESES_IN_SECTOR - 1];
    int32_t continuationBlockAddress;
};

void myAssert(bool condition, const char* message)
{
    if (!condition)
    {
        throw message;
    }
}

class OutOfResourcesException
{};


class IndexManager
{
private:
    size_t count;
public:
    bool* used;
private:
    bool ownTheList;
public:
    IndexManager(size_t count)
        : count(count), used(new bool[count]), ownTheList(true)
    {
        for (size_t i = 0; i < count; i++)
            used[i] = false;
    }
    IndexManager(size_t count, bool* used)
        : count(count), used(used), ownTheList(false)
    {}
    IndexManager(const IndexManager&) = delete;
    size_t Get()
    {
        for (size_t i = 0; i < count; i++)
        {
            if (!used[i])
            {
                used[i] = true;
                return i;
            }
        }
        throw OutOfResourcesException();
        return INT32_MAX;
    }
    void Return(size_t i)
    {
        myAssert(i >= 0 && i < count, "Return out of range");
        myAssert(used[i], "File descriptor is not in use");
        used[i] = false;
    }
    ~IndexManager()
    {
        if (ownTheList)
            delete[] used;
    }
};


class OpenedFile;

class FileBlockIterator
{
    FileContinuation* fileContinuation;
    size_t index;
public:
    FileBlockIterator(FileInfo* fileInfo)
        :
        fileContinuation((FileContinuation*)fileInfo),
        index(fileInfo ? ((char*)fileInfo->blockAddresses - (char*)fileInfo) / sizeof(int32_t) : INT32_MAX)
    {
    }
    bool MoveNext(const TBlkDev& dev, bool writemode, IndexManager& blockManager, OpenedFile& of);
    int32_t Current()
    {
        return fileContinuation->blockAddresses[index];
    }
private:
    void SetCurrent(int32_t blockAddress)
    {
        fileContinuation->blockAddresses[index] = blockAddress;
    }
public:
    void AddBlock(int32_t blockAddress, const TBlkDev& dev, bool writeMode, IndexManager& blockManager, OpenedFile& of)
    {
        SetCurrent(blockAddress);
        MoveNext(dev, writeMode, blockManager, of);
    }
};

class OpenedFile
{
public:
    bool isOpen;
public:
    bool writeMode;
private:
#pragma warning( push )
#pragma warning( disable : 26495)
    unsigned char buffer[BLOCK_SIZE];
    size_t positionInBuffer;
    FileBlockIterator blockIterator;
    size_t unreadSize;
    size_t writtenSize;
    FileInfo* fileInfo;
public:
    FileContinuation* fileContinuation;
    size_t continuationAddr;
private:
    size_t spaceLeftInBuffer()
    {
        return BLOCK_SIZE - positionInBuffer;
    }
    int32_t getNextBlockForWriting(const TBlkDev& dev, IndexManager& blockManager)
    {
        int32_t res;
        //try {
            res = (int32_t)blockManager.Get();
            blockIterator.AddBlock(res, dev, true, blockManager, *this);
        /*}
        catch (OutOfResourcesException)
        {
            std::cout << "Disk is full !!!!!!!!!!" << std::endl;
            throw;
        }*/
        return res;
    }
    int32_t getNextBlockForReading(const TBlkDev& dev, IndexManager& blockManager)
    {
        int32_t res = blockIterator.Current();
        blockIterator.MoveNext(dev, false, blockManager, *this);
        return res;
    }
public:
    void saveContinuation(const TBlkDev& dev)
    {
        if (fileContinuation)
        {
            dev.m_Write(SECTORS_IN_BLOCK * continuationAddr, fileContinuation, 1);
        }
    }
    OpenedFile()
        : isOpen(false),
        writeMode(false),
        positionInBuffer(INT32_MAX),
        blockIterator(nullptr),
        unreadSize(0),
        writtenSize(INT32_MAX),
        fileInfo(nullptr),
        fileContinuation(nullptr),
        continuationAddr(0)
    {
    }
    OpenedFile(bool writeMode, FileInfo* fileInfo) :
        isOpen(true),
        writeMode(writeMode),
        positionInBuffer(writeMode ? 0 : BLOCK_SIZE),
        blockIterator(fileInfo),
        unreadSize(fileInfo->fileSize),
        writtenSize(0),
        fileInfo(fileInfo),
        fileContinuation(nullptr),
        continuationAddr(0)
    {
        myAssert(fileInfo->fileSize >= 0, "file does not exist?");
        memset(buffer, 0, BLOCK_SIZE);
    }
#pragma warning( pop ) 
    size_t ReadData(const TBlkDev& dev, const void* data, size_t len, IndexManager& blockManager)
    {
        if (len > unreadSize)
            len = unreadSize;
        myAssert(isOpen && !writeMode, "file must be opened for reading");
        size_t nRead = 0;
        while (nRead != len)
        {
            if (positionInBuffer == BLOCK_SIZE)
            {
                size_t n = dev.m_Read(
                    SECTORS_IN_BLOCK * getNextBlockForReading(dev, blockManager),
                    buffer,
                    SECTORS_IN_BLOCK
                );
                if (n != SECTORS_IN_BLOCK)
                    throw "This is unexpected";
                positionInBuffer = 0;
            }
            size_t toBeCopied = min(
                len - nRead,
                BLOCK_SIZE - positionInBuffer
            );
            memcpy((char*)data + nRead, buffer + positionInBuffer, toBeCopied);
            positionInBuffer += toBeCopied;
            nRead += toBeCopied;
        }
        unreadSize -= nRead;
        return nRead;
    }
    size_t WriteData(const TBlkDev& dev, const void* data, size_t len, IndexManager& blockManager)
    {
        myAssert(isOpen && writeMode, "file must be opened for writing");
        myAssert(positionInBuffer < BLOCK_SIZE, "position in buffer incorrect");
        size_t nWritten = 0;
        while (nWritten != len)
        {
            size_t toBeWritten = len - nWritten;
            toBeWritten = min(toBeWritten, spaceLeftInBuffer());
            memcpy(
                buffer + positionInBuffer,
                (char*)data + nWritten,
                toBeWritten
            );
            nWritten += toBeWritten;
            positionInBuffer += toBeWritten;
            if (positionInBuffer == BLOCK_SIZE)
            {
                dev.m_Write(SECTORS_IN_BLOCK * getNextBlockForWriting(dev, blockManager), buffer, SECTORS_IN_BLOCK);
                positionInBuffer = 0;
            }
        }
        writtenSize += nWritten;
        fileInfo->fileSize = (int32_t)writtenSize;
        return nWritten;
    }
    void Close(const TBlkDev& dev, IndexManager& blockManager)
    {
        saveContinuation(dev);
        if (positionInBuffer)
        {
            size_t sectorsToWrite = (positionInBuffer + SECTOR_SIZE - 1) / SECTOR_SIZE;
            dev.m_Write(SECTORS_IN_BLOCK * getNextBlockForWriting(dev, blockManager), buffer, sectorsToWrite);
        }
        isOpen = false;
        fileInfo->fileSize = (int32_t)writtenSize;
    }
};

bool FileBlockIterator::MoveNext(const TBlkDev& dev, bool writemode, IndexManager& blockManager, OpenedFile& of)
{
    index++;
    if (index == ADDRESES_IN_SECTOR)
    {
        if (writemode)
        {
            size_t blockAddr = blockManager.Get();
            fileContinuation->continuationBlockAddress = blockAddr;
            of.saveContinuation(dev);
            // TODO pokud stavajici fileContinuation != fileinfo, tak lze uvolnit pamet a mozna by se to melo uvolnovat jeste i jinde
            char* buffer = new char[SECTOR_SIZE];
            memset(buffer, 0, SECTOR_SIZE);
            of.fileContinuation = (FileContinuation*)buffer;
            of.continuationAddr = blockAddr;
            fileContinuation = (FileContinuation*)buffer;
        }
        else
        {
            fileContinuation = new FileContinuation(); // Tohle se taky nikdy neuvolni
            dev.m_Read(
                fileContinuation->continuationBlockAddress * SECTORS_IN_BLOCK,
                (void*)fileContinuation,
                1
            );
        };
        index = 0;
    }
    return fileContinuation->blockAddresses[index] != 0;
}

class ReadException
{};

OpenedFile dummy; // interface to vyzaduje ale nebude pouzit

class CFileSystem
{
private:
    TBlkDev dev;
    OpenedFile openedFile[OPEN_FILES_MAX];
    IndexManager fileDescriptorManager;
    IndexManager fileIndexManager;

    FileInfo* fileInfos;
    IndexManager blockManager;
    bool* getUsedBlocksArray()
    {
        return (bool*)((char*)fileInfos + DIR_ENTRIES_MAX * SECTOR_SIZE);
    }

    static size_t GetBlockCount(const TBlkDev& dev)
    {
        return dev.m_Sectors / SECTORS_IN_BLOCK;
    }

    static size_t ServiceInfoSectorCount(const TBlkDev& dev)
    {
        return
            DIR_ENTRIES_MAX +
            ROUND_TO_MULTIPLE(
                GetBlockCount(dev) * sizeof(bool),
                SECTOR_SIZE
            ) / SECTOR_SIZE;
    }

    static size_t ServiceInfoSize(const TBlkDev& dev)
    {
        return ServiceInfoSectorCount(dev) * SECTOR_SIZE;
    }

    static size_t ServiceInfoBlockCount(const TBlkDev& dev)
    {
        return (ServiceInfoSectorCount(dev) + SECTORS_IN_BLOCK - 1) / SECTORS_IN_BLOCK;
    }

    CFileSystem(const TBlkDev& dev)
        : dev(dev),
        fileDescriptorManager(OPEN_FILES_MAX),
        fileIndexManager(DIR_ENTRIES_MAX),
        fileInfos((FileInfo*)new unsigned char[ServiceInfoSize(dev)]),
        blockManager(GetBlockCount(dev), getUsedBlocksArray())
    {
        size_t sectorCount = ServiceInfoSectorCount(dev);
        size_t res = dev.m_Read(0, (unsigned char*)fileInfos, sectorCount);
        if (res != sectorCount)
            throw new ReadException();
        for (int i = 0; i < DIR_ENTRIES_MAX; i++)
            fileIndexManager.used[i] = fileInfos[i].fileSize >= 0;
    }
    CFileSystem(const CFileSystem&) = delete;
public:
    ~CFileSystem()
    {
        delete[](unsigned char*)fileInfos;
    }
private:
    FileInfo* findFileByName(const char* fileName)
    {
        for (int i = 0; i < DIR_ENTRIES_MAX; i++)
        {
            if (strncmp(fileName, fileInfos[i].fileName, FILENAME_LEN_MAX) == 0 && fileInfos[i].fileSize >= 0)
                return &fileInfos[i];
        }
        return nullptr;
    }
    int MyOpenFile(FileInfo* fileinfo, bool writeMode)
    {
        if (!fileinfo)
            return -1;
        int fd;
        try
        {
            fd = (int)fileDescriptorManager.Get();
        }
        catch (OutOfResourcesException)
        {
            fd = -1;
        }
        if (fd >= 0)
            openedFile[fd] = OpenedFile(writeMode, fileinfo);
        return fd;
    }
    void deleteFile(FileInfo* fileinfo)
    {
        size_t index = fileinfo - fileInfos;
        int nBlocks = ROUND_TO_MULTIPLE(fileinfo->fileSize, BLOCK_SIZE) / BLOCK_SIZE;
        FileBlockIterator iterator(fileinfo);
        for (int i = 0; i < nBlocks; i++)
        {
            blockManager.Return(iterator.Current());
            iterator.MoveNext(dev, false, blockManager, dummy);
        }
        fileinfo->fileSize = -1;
        fileIndexManager.Return(index);
    }
    FileInfo* CreateFile(const char* filename)
    {
        size_t index;
        try
        {
            index = fileIndexManager.Get();
        }
        catch (OutOfResourcesException)
        {
            return nullptr;
        }
        FileInfo* res = fileInfos + index;
        new (res) FileInfo(filename);
        return res;
    }
    int start = 0;
public:
    static bool    CreateFs(const TBlkDev& dev)
    {
        unsigned char* buffer = new unsigned char[ServiceInfoSectorCount(dev) * SECTOR_SIZE];
        memset(buffer, 0, ServiceInfoSize(dev));
        FileInfo* fileInfos = (FileInfo*)buffer;
        for (size_t i = 0; i < DIR_ENTRIES_MAX; i++)
        {
#pragma warning( push )
#pragma warning( disable : 6386)
            fileInfos[i].fileSize = -1;
#pragma warning( pop )
        }
        bool* usedBlocks = (bool*)buffer + SECTOR_SIZE * DIR_ENTRIES_MAX;
        size_t serviceBlocks = ServiceInfoBlockCount(dev);
        for (size_t i = 0; i < GetBlockCount(dev); i++)
            usedBlocks[i] = i < serviceBlocks;
        size_t nWriten = dev.m_Write(0, buffer, ServiceInfoSectorCount(dev));
        if (nWriten != ServiceInfoSectorCount(dev))
        {
            delete[] buffer;
            throw "This is unexpected";
            return false;
        }
        delete[] buffer;
        return true;
    }
    static CFileSystem* Mount(const TBlkDev& dev)
    {
        try
        {
            return new CFileSystem(dev);
        }
        catch (ReadException)
        {
            return nullptr;
        }
    }
    bool           Umount(void)
    {
        for (int i = 0; i < OPEN_FILES_MAX; i++)
            if (openedFile[i].isOpen)
                openedFile[i].Close(dev, blockManager);
        char* buffer = (char*)fileInfos;
        dev.m_Write(0, buffer, ServiceInfoSectorCount(dev));
        return true;
    }
    size_t         FileSize(const char* fileName)
    {
        FileInfo* fileinfo = findFileByName(fileName);
        if (fileinfo)
            return fileinfo->fileSize;
        return SIZE_MAX;
    }
    int            OpenFile(const char* fileName,
        bool              writeMode)
    {
        FileInfo* fileinfo = findFileByName(fileName);
        if (fileinfo)
        {
            if (!writeMode)
            {
                return MyOpenFile(fileinfo, writeMode);
            }
            else
            {
                deleteFile(fileinfo);
            }
        }
        if (writeMode)
            return MyOpenFile(CreateFile(fileName), writeMode);
        else
            return -1;
    }
    bool           CloseFile(int fd)
    {
        if (fd < 0 || fd >= DIR_ENTRIES_MAX)
            return false;
        if (!openedFile[fd].isOpen)
            return false;
        if (openedFile[fd].writeMode)
        {
            openedFile[fd].Close(dev, blockManager);
        }
        fileDescriptorManager.Return(fd);
        return true;
    }
    size_t         ReadFile(int               fd,
        void* data,
        size_t            len)
    {

        return openedFile[fd].ReadData(dev, data, len, blockManager);
    }
    size_t WriteFile(int fd,
        const void* data,
        size_t len
    )
    {
        if (fd < 0 || fd >= OPEN_FILES_MAX)
            return 0;
        return openedFile[fd].WriteData(dev, data, len, blockManager);
    }
    bool           DeleteFile(const char* fileName)
    {
        FileInfo* fileinfo = findFileByName(fileName);
        if (!fileinfo)
            return false;
        else
        {
            deleteFile(fileinfo);
            return true;
        }
    }
    bool           FindFirst(TFile& file)
    {
        start = 0;
        return FindNext(file);
    }
    bool           FindNext(TFile& file)
    {
        for (int i = start; i < DIR_ENTRIES_MAX; i++)
        {
            FileInfo* fileinfo = &fileInfos[i];
            if (fileinfo->IsFileHere())
            {
                file.m_FileSize = fileinfo->fileSize;
                strncpy(file.m_FileName, fileinfo->fileName, FILENAME_LEN_MAX + 1);
                start = i + 1;
                return true;
            }
        }
        return false;
    }
};


#ifndef __PROGTEST__
#include "simple_test_mega.h"
#endif /* __PROGTEST__ */
