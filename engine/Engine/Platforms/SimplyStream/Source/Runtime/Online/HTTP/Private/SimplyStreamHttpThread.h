#pragma once

#include "CoreMinimal.h"
#include "HttpThread.h"

class FHttpRequestCommon;
struct FetchData;

class FSimplyStreamHttpThread : public FLegacyHttpThread {
public:
	FSimplyStreamHttpThread();
protected:
	//~ Begin FHttpThread Interface
	virtual void HttpThreadTick(float DeltaSeconds) override;
	virtual bool StartThreadedRequest(FHttpRequestCommon* Request) override;
	virtual void CompleteThreadedRequest(FHttpRequestCommon* Request) override;
	//~ End FHttpThread Interface
};