// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

public class SQLiteCore_SimplyStream : SQLiteCore
{
	public SQLiteCore_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDefinitions.Add("SQLITE_THREADSAFE=0"); // No threading on SimplyStream
	}
}
