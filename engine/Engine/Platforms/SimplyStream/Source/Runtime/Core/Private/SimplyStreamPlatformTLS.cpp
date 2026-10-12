#include "SimplyStreamPlatformTLS.h"
#include "Containers/Array.h"
#include <pthread.h>

uint32 FSimplyStreamTLS::GetCurrentThreadId(void)
{
	return pthread_self();
}

uint32 FSimplyStreamTLS::AllocTlsSlot(void)
{
	pthread_key_t Key = 0;
	if (pthread_key_create(&Key, nullptr) != 0)
	{
		return static_cast<uint32>(INDEX_NONE); // matches the Windows TlsAlloc() retval.
	}

	//EM_ASM(console.log('AllocTlsSlot with key', $0, $1), threadId, Key);
	// by allocating another one first (so we get another value) and releasing existing key.
	if (static_cast<uint32>(Key) == static_cast<uint32>(INDEX_NONE))
	{
		pthread_key_t NewKey = 0;
		int SecondKeyAllocResult = pthread_key_create(&NewKey, nullptr);
		// discard the previous one
		pthread_key_delete((pthread_key_t)Key);

		if (SecondKeyAllocResult != 0)
		{
			// could not alloc the second key, treat this as an error
			return static_cast<uint32>(INDEX_NONE); // matches the Windows TlsAlloc() retval.
		}

		// check that we indeed got something different
		checkf(NewKey != static_cast<uint32>(INDEX_NONE), TEXT("Could not allocate a usable TLS slot id."));

		Key = NewKey;
	}

	return Key;
}

void FSimplyStreamTLS::SetTlsValue(uint32 SlotIndex, void* Value)
{
	pthread_setspecific((pthread_key_t)SlotIndex, Value);
}

void* FSimplyStreamTLS::GetTlsValue(uint32 SlotIndex)
{
	return pthread_getspecific((pthread_key_t)SlotIndex);
}

void FSimplyStreamTLS::FreeTlsSlot(uint32 SlotIndex)
{
	pthread_key_delete((pthread_key_t)SlotIndex);
}