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
#include <cstdio>
#include <string>
#include <ctime>

#define DISK_SECTORS 8192
static FILE *g_Fp = NULL;

//-------------------------------------------------------------------------------------------------
/** Sample read_iterator_sector reading function. The function will be called by your fs driver implementation.
 * Notice, the function is not called directly. Instead, the function will be invoked indirectly
 * through function pointer in the TBlkDev structure.
 */
static size_t diskRead(size_t sectorNr,
                       void *data,
                       size_t sectorCnt) {
    if (g_Fp == NULL
        || sectorNr + sectorCnt > DISK_SECTORS)
        return 0;
    fseek(g_Fp, sectorNr * SECTOR_SIZE, SEEK_SET);
    return fread(data, SECTOR_SIZE, sectorCnt, g_Fp);
}
//-------------------------------------------------------------------------------------------------
/** Sample read_iterator_sector writing function. Similar to diskRead
 */
static size_t diskWrite(size_t sectorNr, const void *data, size_t sectorCnt) {
    if (g_Fp == NULL
        || sectorNr + sectorCnt > DISK_SECTORS)
        return 0;
    fseek(g_Fp, sectorNr * SECTOR_SIZE, SEEK_SET);
    return fwrite(data, SECTOR_SIZE, sectorCnt, g_Fp);
}
//-------------------------------------------------------------------------------------------------
/** A function which creates the file needed for the read_iterator_sector reading/writing functions above.
 * This function is only needed for the particular implementation above. It could be understand as
 * "buying a new disk".
 */
static TBlkDev createDisk(void) {
    char buffer[SECTOR_SIZE];

    memset(buffer, 0, sizeof(buffer));
    g_Fp = fopen("/tmp/disk_content", "w+b");
    if (!g_Fp)
        throw "Error creating backed block device";

    for (int i = 0; i < DISK_SECTORS; i++)
        if (fwrite(buffer, sizeof(buffer), 1, g_Fp) != 1)
            throw "Error creating backed block device";

    TBlkDev res;
    res.m_Sectors = DISK_SECTORS;
    res.m_Read = diskRead;
    res.m_Write = diskWrite;
    return res;
}
//-------------------------------------------------------------------------------------------------
/** A function which opens the files_total needed for the read_iterator_sector reading/writing functions above.
 * This function is only needed for the particular implementation above. It could be understand as
 * "turning the computer on".
 */
static TBlkDev openDisk(void) {
    g_Fp = fopen("/tmp/disk_content", "r+b");
    if (!g_Fp)
        throw "Error opening backend block device";
    fseek(g_Fp, 0, SEEK_END);
    if (ftell(g_Fp) != DISK_SECTORS * SECTOR_SIZE) {
        fclose(g_Fp);
        g_Fp = NULL;
        throw "Error opening backend block device";
    }

    TBlkDev res;
    res.m_Sectors = DISK_SECTORS;
    res.m_Read = diskRead;
    res.m_Write = diskWrite;
    return res;
}
//-------------------------------------------------------------------------------------------------
/** A function which releases resources allocated by openDisk/createDisk
 */
static void doneDisk(void) {
    if (g_Fp) {
        fclose(g_Fp);
        g_Fp = NULL;
    }
}

//-------------------------------------------------------------------------------------------------
static void testMkFs(void) {
    /* Create the disk backend and format it using your FsCreate call
     */

    assert (CFileSystem::CreateFs(createDisk()));
    doneDisk();
}

//-------------------------------------------------------------------------------------------------
//string sample = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void test_zakladnich_dat(CFileSystem *fs) {
    printf("\ntest - zaklad\n");
    uint8_t buffer_in[512], buffer_out[512];
    int fd;
    // test zapisu a cteni s porovnanim obsahu do velikosti 1 secktoru
    for (unsigned char &i : buffer_in) {
        i = 255;
    }
    for (int i = 0; i < SECTOR_SIZE; ++i) {
        fd = fs->OpenFile("test0", true);
        assert ((int) fs->WriteFile(fd, buffer_in, i) == i);
        assert (fs->CloseFile(fd));
        for (int j = 0; j < SECTOR_SIZE; ++j) {
            fd = fs->OpenFile("test0", false);
            int out;
            if (j <= i)
                out = j;
            else
                out = i;
            //printf("%d - %d\n",i,j);
            assert ((int) fs->ReadFile(fd, buffer_out, j) == out);
            for (int k = 0; k < out; ++k) {
                if (buffer_in[k] != buffer_out[k])
                    printf("    Data not match!\n");
            }
            assert (fs->CloseFile(fd));
        }
    }
    assert(fs->DeleteFile("test0"));
}

static void test_max_file_count(CFileSystem *fs) {
    printf("\ntest - max file count\n");
    uint8_t buffer_in[512];
    int fd, out = 1234;
    bool outb = true;
    for (int i = 0; i < DIR_ENTRIES_MAX * 2; ++i) {
        if (i > DIR_ENTRIES_MAX - 1) {
            out = 0;
            outb = false;
        }
        fd = fs->OpenFile(std::to_string(i).c_str(), true);
        if (fd >= 0)
            printf("    Created: %d\n", i);
        assert ((int) fs->WriteFile(fd, buffer_in, 1234) == out);
        assert (fs->CloseFile(fd) == outb);
    }
    printf("\n");
    for (int i = 0; i < DIR_ENTRIES_MAX * 2; ++i) {
        if (fs->DeleteFile(std::to_string(i).c_str()))
            printf("    Deleted: %d\n", i);
    }
}


static void test_max_file_open_count(CFileSystem *fs) {
    printf("\ntest - max open file count\n");
    int fd;
    for (int i = 0; i < OPEN_FILES_MAX * 2; ++i) {
        fd = fs->OpenFile(std::to_string(i).c_str(), true);
        if (fd > -1)
            printf("    Open succ: %d\n", i);
        else
            printf("    Open fail: %d\n", i);
        //assert (fs->WriteFile(fd, buffer_in, 1234) == out);
    }
    printf("\n");
    for (int i = 0; i < OPEN_FILES_MAX * 2; ++i) {
        fs->DeleteFile(std::to_string(i).c_str());
        printf("    Deleted: %d\n", i);
    }
}

static void test_velka_data1(CFileSystem *fs) {
    printf("\ntest - velka data 1\n");
    size_t buffer_size = 65536;
    uint8_t buffer_in[65536], buffer_out[65536];
    int fd;

    for (int i = 0; i < (int) (DISK_SECTORS * SECTOR_SIZE / buffer_size) + 5; ++i) {
        if (i == 51)
            i = 51;
        fd = fs->OpenFile(std::to_string(i).c_str(), true);
        for (unsigned char &k:buffer_in) {
            k = i;
        }
        if (fd != -1) {
            size_t inc = ((i * 123 + 321) * 100000) % (5120);
            size_t sum = 0;
            for (int j = 0; j < 14; ++j) {
                if (fs->WriteFile(fd, buffer_in, inc))
                    sum += inc;
            }
            assert (fs->CloseFile(fd) == true);
            printf("    %zu written to %d\n", sum, i);
            fd = fs->OpenFile(std::to_string(i).c_str(), false);
            size_t tmp = fs->ReadFile(fd, buffer_out, buffer_size);
            printf("    %zu read from %d\n", tmp, i);
            for (int j = 0; j < (int) tmp; ++j) {
                if (buffer_in[j] != buffer_out[j])
                    printf("    Data not match! - %d\n", j);
            }
            assert (fs->CloseFile(fd) == true);
        }
    }
    printf("\n");
    for (int i = 0; i < (int) (DISK_SECTORS * SECTOR_SIZE / buffer_size) + 5; ++i) {
        fs->DeleteFile(std::to_string(i).c_str());
        printf("    Deleted: %d\n", i);
    }

}

static void test_velka_data2(CFileSystem *fs) {
    printf("\ntest - velka data 2\n");
    size_t buffer_size = 524288;
    uint8_t buffer_in[524288], buffer_out[524288];
    int fd;

    for (int i = 0; i < (int) (DISK_SECTORS * SECTOR_SIZE / buffer_size) + 5; ++i) {
        fd = fs->OpenFile(std::to_string(i).c_str(), true);
        for (unsigned char &k:buffer_in) {
            k = i;
        }
        if (fd != -1) {
            size_t inc = ((i * 123 + 321) * 100000) % (buffer_size / 16);
            size_t sum = 0;
            for (int j = 0; j < 16; ++j) {
                if (fs->WriteFile(fd, buffer_in, inc))
                    sum += inc;
            }
            assert (fs->CloseFile(fd) == true);
            printf("    %zu written to %d\n", sum, i);
            fd = fs->OpenFile(std::to_string(i).c_str(), false);
            size_t tmp = fs->ReadFile(fd, buffer_out, buffer_size);
            printf("    %zu read from %d\n", tmp, i);
            for (int j = 0; j < (int) tmp; ++j) {
                if (buffer_in[j] != buffer_out[j])
                    printf("    Data not match!\n");
            }
            assert (fs->CloseFile(fd) == true);
        }
    }
    printf("\n");
    for (int i = 0; i < (int) (DISK_SECTORS * SECTOR_SIZE / buffer_size) + 5; ++i) {
        fs->DeleteFile(std::to_string(i).c_str());
        printf("    Deleted: %d\n", i);
    }
}

static void test_velka_data3(CFileSystem *fs) {
    printf("\ntest - velka data 3\n");
    size_t buffer_size = 524288;
    uint8_t buffer_in[524288], buffer_out[524288];
    for (unsigned char &k:buffer_in) {
        k = 255;
    }
    int fd;
    size_t inc[5] = {165324, 495173, 52400, 68931, 99623};
    for (auto &i:inc) {
        size_t sum = 0, tmp = 0;
        // write
        fd = fs->OpenFile(std::to_string(i).c_str(), true);
        while ((tmp = fs->WriteFile(fd, buffer_in, i)) > 0)
            sum += tmp;
        assert (fs->CloseFile(fd));
        printf("    %zu written to %s\n", sum, std::to_string(i).c_str());
        // read
        fd = fs->OpenFile(std::to_string(i).c_str(), false);
        sum = 0;
        while ((tmp = fs->ReadFile(fd, buffer_out, buffer_size)) > 0) {
            sum += tmp;
        }
        printf("    %zu read from %s\n", sum, std::to_string(i).c_str());
        //assert (fs->CloseFile(fd) == true);
        fs->DeleteFile(std::to_string(i).c_str());
        printf("    Deleted: %s\n\n", std::to_string(i).c_str());
    }
}

void test_cteni_nexistujicich_souboru(CFileSystem *fs) {
    printf("\ntest - cteni neexistujicich\n");
    for (int i = 0; i < 10; ++i) {
        assert(fs->OpenFile(std::to_string(i).c_str(), false) == -1);
    }
}

void test_mazani_nexistujicich_souboru(CFileSystem *fs) {
    printf("\ntest - mayani neexistujicich\n");
    for (int i = 0; i < 10; ++i) {
        assert(fs->DeleteFile(std::to_string(i).c_str()) == false);
    }
}

void test_mazani_uprostred(CFileSystem *fs) {
    printf("\ntest - mazani uprostred\n");
    uint8_t buffer_in[4096], buffer_out[4096];
    for (auto &i:buffer_in)
        i = 0;
    int fd;
    fd = fs->OpenFile("0", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("1", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("2", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("3", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("4", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("5", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("6", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));

    TFile info;

    assert(fs->DeleteFile("1"));
    printf("    Deleted 1\n");
    for (bool found = fs->FindFirst(info); found; found = fs->FindNext(info))
        printf("    %-30s %6zd\n", info.m_FileName, info.m_FileSize);
    assert(fs->DeleteFile("4"));
    printf("    Deleted 4\n");
    for (bool found = fs->FindFirst(info); found; found = fs->FindNext(info))
        printf("    %-30s %6zd\n", info.m_FileName, info.m_FileSize);
    assert(fs->DeleteFile("3"));
    printf("    Deleted 3\n");

    assert(fs->DeleteFile("0"));
    assert(fs->DeleteFile("2"));
    assert(fs->DeleteFile("5"));
    assert(fs->DeleteFile("6"));
}

void test_male_data(CFileSystem *fs) {
    printf("\ntest - mala data\n");
    uint8_t buffer_in[2048], buffer_out[2048];
    int fd;
    for (auto &i:buffer_in)
        i = 0;
    fd = fs->OpenFile("test", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("test", false);
    assert((int) fs->ReadFile(fd, buffer_out, 1234) == 1234);
    for (int i = 0; i < 1234; ++i) {
        if (buffer_in[i] != buffer_out[i])
            printf("    Data does not match!\n");
    }
    assert(fs->DeleteFile("test"));
}

void test_base(CFileSystem *fs) {
    printf("\ntest - base\n");
    size_t var = 69;
    uint8_t buffer_in[4096], buffer_out[4096];
    for (auto &i:buffer_in)
        i = var;
    int fd;
    fd = fs->OpenFile("test", true);
    assert ((int) fs->WriteFile(fd, buffer_in, 1234) == 1234);
    assert (fs->CloseFile(fd));
    fd = fs->OpenFile("test", false);
    assert((int) fs->ReadFile(fd, buffer_out, 654) == 654);
    for (int i = 0; i < 654; ++i) {
        assert(buffer_out[i] == var);
    }
    assert((int) fs->ReadFile(fd, buffer_out + 654, 400) == 400);
    for (int i = 654; i < 654 + 400; ++i) {
        assert(buffer_out[i] == var);
    }
    assert((int) fs->ReadFile(fd, buffer_out + 654 + 400, 285) == 180);
    for (int i = 654 + 400; i < 654 + 400 + 180; ++i) {
        assert(buffer_out[i] == var);
    }
    assert(fs->DeleteFile("test"));
}

void test_open_random(CFileSystem *fs) {
    printf("\ntest - open random\n");
    uint8_t buffer_in[524288], buffer_out[524288];
    int fd;
    // opening
    fs->OpenFile("0", true);
    fs->OpenFile("1", true);
    fs->OpenFile("2", true);
    fs->OpenFile("3", true);
    fs->OpenFile("4", true);
    fs->OpenFile("5", true);
    fs->OpenFile("6", true);
    fs->OpenFile("7", true);

    for (int i = 0; i < 100; ++i) {
        assert(fs->WriteFile(rand() % 8, buffer_in, 1234) == 1234);
    }

    fs->CloseFile(0);
    fs->CloseFile(1);
    fs->CloseFile(2);
    fs->CloseFile(3);
    fs->CloseFile(4);
    fs->CloseFile(5);
    fs->CloseFile(6);
    fs->CloseFile(7);

    /*fs->OpenFile("0", false);
    fs->OpenFile("1", true);
    fs->OpenFile("2", false);
    fs->OpenFile("3", true);
    fs->OpenFile("4", false);
    fs->OpenFile("5", true);
    fs->OpenFile("6", false);
    fs->OpenFile("7", true);

    assert(fs->ReadFile(4, buffer_out, 165) == 165);
    assert(fs->ReadFile(2, buffer_out, 33) == 33);
    assert(fs->ReadFile(2, buffer_out, 48) == 48);
    assert(fs->WriteFile(7, buffer_in, 1234) == 1234);
    assert(fs->WriteFile(0, buffer_in, 826) == 0);
    assert(fs->WriteFile(5, buffer_in, 2048) == 2048);
    assert(fs->CloseFile(5));
    fs->OpenFile("5", false);
    assert(fs->ReadFile(5, buffer_out, 1500) == 1500);
    assert(fs->WriteFile(1, buffer_in, 9236) == 9236);
    assert(fs->WriteFile(1, buffer_in, 10) == 10);
    assert(fs->WriteFile(7, buffer_in, 2048) == 2048);
    assert(fs->CloseFile(1));
    fs->OpenFile("1", true);
    assert(fs->WriteFile(1, buffer_in, 10) == 10);

    fs->CloseFile(7);
    fs->CloseFile(0);
    fs->CloseFile(4);
    fs->CloseFile(3);
    fs->CloseFile(2);
    fs->CloseFile(5);
    fs->CloseFile(1);
    fs->CloseFile(6);

    fs->OpenFile("0", false);
    fs->OpenFile("6", false);
    fs->OpenFile("4", false);
    fs->OpenFile("2", false);
    fs->OpenFile("3", true);
    fs->OpenFile("7", true);
    fs->OpenFile("5", true);
    fs->OpenFile("1", true);

    assert(fs->ReadFile(4, buffer_out, 165) == 0);
    assert(fs->ReadFile(2, buffer_out, 33) == 33);
    assert(fs->ReadFile(2, buffer_out, 48) == 48);
    assert(fs->WriteFile(7, buffer_in, 1234) == 1234);
    assert(fs->WriteFile(0, buffer_in, 826) == 0);
    assert(fs->WriteFile(5, buffer_in, 2048) == 2048);
    assert(fs->CloseFile(5));
    fs->OpenFile("5", false);
*/
    fs->DeleteFile("5");
    fs->DeleteFile("1");
    fs->DeleteFile("7");
    fs->DeleteFile("3");
    fs->DeleteFile("4");
    fs->DeleteFile("2");
    fs->DeleteFile("0");
    fs->DeleteFile("6");

}

void test_cely_disk(CFileSystem *fs) {
    printf("\ntest - cely disk\n");
    int fd0;
    size_t buffer_size = 512, tmp = 0, sum = 0;
    uint8_t buffer_in[524288], buffer_out[524288];

    time_t timer, diff;

    //printf("    Pocet free sektoru pred open:               %zu\n", fs->get_free_sectors());
    fd0 = fs->OpenFile("test", true);
    //printf("    Pocet free sektoru po open:                 %zu\n", fs->get_free_sectors());

    // timer write
    timer = time(NULL);
    while ((tmp = fs->WriteFile(fd0, buffer_in, buffer_size)) > 0) {
        sum += tmp;
    }  //1024
    diff = time(NULL);
    printf("    Zapis celeho diku trval:                    %zu s\n", diff - timer);
    //printf("    Pocet free sektoru po zaplneni disku:       %zu\n", fs->get_free_sectors());
    fs->CloseFile(fd0);
    fd0 = fs->OpenFile("test", false);


    // timer read
    timer = time(NULL);
    while (fs->ReadFile(fd0, buffer_in, buffer_size) > 0) {}
    diff = time(NULL);
    printf("    Cteni celeho diku trval:                    %zu s\n", diff - timer);
    fs->CloseFile(fd0);


    //printf("    Pocet free sektoru po zavreni souboru:      %zu\n", fs->get_free_sectors());
    fs->DeleteFile("test");
    //printf("    Pocet free sektoru po odstraneni souboru:   %zu\n", fs->get_free_sectors());
}

static void test_vse() {
    TFile info;
    CFileSystem *fs = CFileSystem::Mount(openDisk());
    assert(fs);

    //test_write_sector(fs);
    test_mazani_uprostred(fs);
    test_base(fs);
    test_male_data(fs);
    assert(fs->Umount());
    delete fs;
    doneDisk();
    fs = CFileSystem::Mount(openDisk());
    assert(fs);
    test_zakladnich_dat(fs);
    test_max_file_count(fs);
    test_max_file_open_count(fs);
    test_velka_data1(fs);
    test_velka_data2(fs);
    assert(fs->Umount());
    delete fs;
    doneDisk();
    fs = CFileSystem::Mount(openDisk());
    assert(fs);
    test_velka_data3(fs);
    test_cteni_nexistujicich_souboru(fs);
    test_mazani_nexistujicich_souboru(fs);
    test_cely_disk(fs);
    test_open_random(fs);

    assert(fs->Umount());
    delete fs;
    doneDisk();
    fs = CFileSystem::Mount(openDisk());
    assert(fs);
    for (bool found = fs->FindFirst(info); found; found = fs->FindNext(info))
        printf("%-30s %6zd\n", info.m_FileName, info.m_FileSize);

    assert(fs->Umount());
    delete fs;
    doneDisk();
}

int main()
{
  testMkFs();
  test_vse();
}

//-------------------------------------------------------------------------------------------------

