// Copyright Epic Games, Inc. All Rights Reserved.

#	include <arpa/inet.h>
#	include <fcntl.h>
#	include <netdb.h>
#	include <netinet/tcp.h>
#	include <poll.h>
#	include <sys/select.h>
#	include <sys/socket.h>
#	include <unistd.h>
namespace UE::IoStore::HTTP
{
	using SocketType = int;
	using MsgFlagType = int;
	static int32 closesocket(int32 Socket) { return close(Socket); }
}
#	define IAS_HTTP_USE_POLL
