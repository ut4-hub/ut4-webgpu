//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

namespace UnrealBuildTool.Rules
{
    public class ResonanceAudio_SimplyStream : ResonanceAudio
    {
		protected virtual bool bSupportsProceduralMesh { get { return false; } }

		public ResonanceAudio_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
        {
        }
    }
}
