/***********************************************************************************
*	sys_arch.c is the bridge between bear and the LwIP stack
*	Author: Martin Osterloh
*
***********************************************************************************/

#include <sbin/lwip/sys.h>
#include <sbin/lwip/sys_arch.h>

/*
 * These are all dummies to shut lwip up
 * We are not using lwip as a system component.
 * We rather use its source files as needed
 */
 

void sys_init(void)
{

}

/*

	SEMAPHORE FUNCTIONS
	
*/

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
}

void sys_sem_signal(sys_sem_t *sem)
{
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	return 0; 
}

/*

	MAILBOX FUNCTIONS
	
*/

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
	return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
	return ERR_OK;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
	return 0;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
	return 0;
}

/*

	THREAD FUNCTIONS
	
*/
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg, int stacksize, int prio)
{
	sys_thread_t new_thread;
	return new_thread;
}

/*

	CRITICAL REGION PROTECTION FUNCTIONS
	
*/

sys_prot_t sys_arch_protect( void )
{
	/* lock the mutexes */
	/* mutex_lock( sys_arch_mutex ); */
	return 0;
}

void sys_arch_unprotect( sys_prot_t pval )
{
	/* release the mutexes */
	/* mutex_release( sys_arch_mutex ); */
}


