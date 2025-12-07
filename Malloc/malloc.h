#ifndef _malloc_H
#define _malloc_H


#include "system.h"


#ifndef NULL
#define NULL 0
#endif

//����2���ڴ��
#define SRAMIN	 0		//�ڲ��ڴ��

#define SRAMBANK 	1	//����֧�ֵ�SRAM����.	

//mem1内存参数设定.mem1完全处于内部SRAM里面.
#define MEM1_BLOCK_SIZE			32  	  						//内存块大小为32字节
#define MEM1_MAX_SIZE			22*1024  						//最大管理内存 22K (够19.2KB图像缓冲+余量)
#define MEM1_ALLOC_TABLE_SIZE	MEM1_MAX_SIZE/MEM1_BLOCK_SIZE 	//内存表大小


//�ڴ����������   ����ָ��
struct malloc_cortol_struct
{
	void (*init)(u8);					//��ʼ��
	u8 (*perused)(u8);		  	    	//�ڴ�ʹ����
	u8 	*membase[SRAMBANK];				//�ڴ�� ����SRAMBANK��������ڴ�
	u16 *memmap[SRAMBANK]; 				//�ڴ����״̬��
	u8  memrdy[SRAMBANK]; 				//�ڴ�����Ƿ����
};
extern struct malloc_cortol_struct malloc_cortol;	 //��mallco.c���涨��

void my_mem_set(void *s,u8 c,u32 num)  ;	//�����ڴ�
void my_mem_cpy(void *des,void *src,u32 len)  ;//�����ڴ�     
void my_mem_init(u8 memx);				//�ڴ������ʼ������(��/�ڲ�����)
u32 my_mem_malloc(u8 memx,u32 size);	//�ڴ����(�ڲ�����)
u8 my_mem_free(u8 memx,u32 offset);		//�ڴ��ͷ�(�ڲ�����)
u8 my_mem_perused(u8 memx);				//����ڴ�ʹ����(��/�ڲ�����) 
////////////////////////////////////////////////////////////////////////////////
//�û����ú���
void myfree(u8 memx,void *paddr) ;  			//�ڴ��ͷ�(�ⲿ����)
void *mymalloc(u8 memx,u32 size);			//�ڴ����(�ⲿ����)
void *myrealloc(u8 memx,void *paddr,u32 size) ;//���·����ڴ�(�ⲿ����)


#endif
