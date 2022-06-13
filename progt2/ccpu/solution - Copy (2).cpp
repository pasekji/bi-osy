#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include "common.h"
using namespace std;
#endif /* __PROGTEST__ */

pthread_mutex_t lock;
bool* freePages;
uint32_t global_totalPages;
unsigned int nPouzitychStranek;
unsigned int nUvolnenychStranek;
unsigned int nRezervovanychStranek = 1;

class MyVector
{
    int count;
    pthread_mutex_t lock;
public:
    pthread_mutex_t ukonceni;
    MyVector()
    {
        count = 0;
    }
    void push_back()
    {
        pthread_mutex_lock(&lock);
        if (count == 0)
            pthread_mutex_lock(&ukonceni);
        count++;
        pthread_mutex_unlock(&lock);
    }
    void pop_back()
    {
        pthread_mutex_lock(&lock);
        count--;
        pthread_mutex_unlock(&lock);
        if (count == 0)
            pthread_mutex_unlock(&ukonceni);
    }
};

MyVector listOfThreads;

bool zarezervujDanyPocetStranek(uint32_t newPages)
{
    pthread_mutex_lock(&lock);
    if (nRezervovanychStranek + newPages <= global_totalPages)
    {
        nRezervovanychStranek += newPages;
        pthread_mutex_unlock(&lock);
        return true;
    }
    pthread_mutex_unlock(&lock);
    return false;
}

uint32_t getFreePage()
{
    pthread_mutex_lock(&lock);
    for (uint32_t i = 0; i < global_totalPages; i++)
    {
        if (freePages[i])
        {
            freePages[i] = false;
            nPouzitychStranek++;
            pthread_mutex_unlock(&lock);
            return i;
        }
    }
    throw "error";
    pthread_mutex_unlock(&lock);
    return 0;
}

void setPageAsFree(uint32_t page)
{
    pthread_mutex_lock(&lock);
    if (freePages[page] == true)
    {
        throw "error";
    }
    freePages[page] = true;
    nUvolnenychStranek++;
    nRezervovanychStranek--;
    pthread_mutex_unlock(&lock);
}

class CProcess;
struct NewProcessData
{
    void * processArg;
    CCPU* process;
    void (* entryPoint) ( CCPU *, void * );
};

void * worker(void * arg)
{
    NewProcessData* data = (NewProcessData*)arg;
    data->entryPoint(data->process, data->processArg);
    delete data->process;
    listOfThreads.pop_back();
    return NULL;
}

class CProcess : public CCPU
{
private:
    void copyPage(uint32_t dst, uint32_t src)
    {
        for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
        {
            uint32_t* pDst = (uint32_t*)(m_MemStart + dst*4096 + 4*i);
            uint32_t* pSrc = (uint32_t*)(m_MemStart + src*4096 + 4*i);
            *pDst = *pSrc;
        }
    }
    void nothingIsPresent(uint32_t page)
    {
        for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
        {
            uint32_t* r = (uint32_t*)(m_MemStart + page*4096 + 4*i);
            *r = 0;
        }
    }
    uint32_t getFreePageDir()
    {
        uint32_t res = getFreePage();
        nothingIsPresent(res);
        return res;
    }

    uint32_t logicalPageToPhysical(uint32_t logicalPage)
    {
        uint32_t indexPodadresare = logicalPage / PAGE_DIR_ENTRIES;
        uint8_t* q = m_MemStart + m_PageTableRoot + 4*indexPodadresare;
        uint32_t adresaPodadresare = (*(uint32_t*)q) >> 12;
        uint8_t* p = m_MemStart + adresaPodadresare*PAGE_SIZE + 4*(logicalPage % PAGE_DIR_ENTRIES);
        return *(uint32_t*)p >> 12;
    }

    uint32_t pagesLimit = 0;
    void addPageToInternalStructures(uint32_t pageIndex)
    {
        uint32_t indexPodadresare = pagesLimit / PAGE_DIR_ENTRIES;
        if (pagesLimit % PAGE_DIR_ENTRIES == 0)
        {
            uint32_t adresaPodadresare = getFreePageDir();
            uint8_t* p = m_MemStart + m_PageTableRoot + 4*indexPodadresare;
            uint32_t hodnota = (adresaPodadresare << 12) + BIT_USER + BIT_WRITE + BIT_PRESENT;
            *(uint32_t*)p = hodnota;
        }
        {
            uint8_t* q = m_MemStart + m_PageTableRoot + 4*indexPodadresare;
            uint32_t adresaPodadresare = (*(uint32_t*)q) >> 12;
            uint8_t* p = m_MemStart + adresaPodadresare*PAGE_SIZE + 4*(pagesLimit % PAGE_DIR_ENTRIES);
            *(uint32_t*)p = (pageIndex << 12) + BIT_DIRTY + BIT_REFERENCED + BIT_USER + BIT_WRITE + BIT_PRESENT;
        }
        pagesLimit++;
    }
    void deletePage()
    {
        pagesLimit--;
        uint32_t indexPodadresare = pagesLimit / PAGE_DIR_ENTRIES;
        {
            uint8_t* q = m_MemStart + m_PageTableRoot + 4*indexPodadresare;
            uint32_t adresaPodadresare = (*(uint32_t*)q) >> 12;
            uint8_t* p = m_MemStart + adresaPodadresare*PAGE_SIZE + 4*(pagesLimit % PAGE_DIR_ENTRIES);
            setPageAsFree(*(uint32_t*)p >> 12);
            *(uint32_t*)p = *(uint32_t*)p - BIT_PRESENT;
        }
        if (pagesLimit % PAGE_DIR_ENTRIES == 0)
        {
            uint8_t* p = m_MemStart + m_PageTableRoot + 4*indexPodadresare;
            setPageAsFree(*(uint32_t*)p >> 12);
            *(uint32_t*)p = *(uint32_t*)p - BIT_PRESENT;
        }
    }
public:
    CProcess(uint8_t* m_MemStart, uint32_t m_PageTableRoot)
        : CCPU(m_MemStart, m_PageTableRoot)
    {
        nothingIsPresent(m_PageTableRoot >> 12);
    }
    virtual uint32_t         GetMemLimit                   ( void ) const
    {
         return pagesLimit;
    }
    virtual bool             SetMemLimit                   ( uint32_t          pages )
    {
        for (;pages < pagesLimit;)
        {
            deletePage();
        }
        uint32_t zbyvaNaalokovat = pages - pagesLimit;
        if (zbyvaNaalokovat == 0) return true;
        zbyvaNaalokovat += ((pages + PAGE_DIR_ENTRIES-1) / PAGE_DIR_ENTRIES) - ((pagesLimit + PAGE_DIR_ENTRIES-1) / PAGE_DIR_ENTRIES);
        if (zarezervujDanyPocetStranek(zbyvaNaalokovat))
        {
            for (; pagesLimit < pages; )
            {
                uint32_t pagePhysicalIndex = getFreePage();
                addPageToInternalStructures(pagePhysicalIndex);
            }
            return true;
        }
        else
            return false;
    }
    virtual bool NewProcess(
        void * processArg,
        void (* entryPoint) ( CCPU *, void * ),
        bool copyMem
    )
    {
        uint32_t zarezervovat = 1;
        if (copyMem)
            zarezervovat += pagesLimit + ((pagesLimit + PAGE_DIR_ENTRIES -1) / PAGE_DIR_ENTRIES);
        if (zarezervujDanyPocetStranek(zarezervovat)) {
            auto *process = new CProcess(m_MemStart, 4096 * getFreePage());
            if (copyMem) {
                for (uint32_t i = 0; i < pagesLimit; i++) {
                    uint32_t pagePhysicalIndex = getFreePage();
                    process->addPageToInternalStructures(pagePhysicalIndex);
                    copyPage(pagePhysicalIndex, logicalPageToPhysical(i));
                }
            }
            NewProcessData *newProcessData = new NewProcessData{processArg, (CCPU *) process, entryPoint};

            pthread_t *pThread = new pthread_t();
            listOfThreads.push_back();
            pthread_create(pThread, NULL, &worker, (void *) newProcessData);
            return true;
        }
        return false;
    }
    virtual ~CProcess()
    {
        SetMemLimit(0);
        setPageAsFree(m_PageTableRoot >> 12);
    }
};

void MemMgr( void * mem,
    uint32_t totalPages,
    void * processArg,
    void (* mainProcess) ( CCPU *, void * )
)
{
    nPouzitychStranek = 0;
    nUvolnenychStranek = 0;
    nRezervovanychStranek = 1;
    global_totalPages = totalPages;
    freePages = new bool[totalPages];
    for (uint32_t i = 0; i < totalPages; i++)
        freePages[i] = true;
    auto* init = new CProcess((uint8_t*) mem, 4096*getFreePage());
    listOfThreads.push_back();
    mainProcess(init, processArg);
    listOfThreads.pop_back();
    pthread_mutex_lock(&listOfThreads.ukonceni);
    pthread_mutex_unlock(&listOfThreads.ukonceni);
}
