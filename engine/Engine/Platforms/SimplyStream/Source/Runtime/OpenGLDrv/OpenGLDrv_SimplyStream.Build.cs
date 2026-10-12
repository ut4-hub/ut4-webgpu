using System.IO;

namespace UnrealBuildTool.Rules
{
	public class OpenGLDrv_SimplyStream : OpenGLDrv
	{
		public OpenGLDrv_SimplyStream(ReadOnlyTargetRules Target) : base(Target)
		{
			// base will set this, but we don't want it
			PrivateDependencyModuleNames.Remove("OpenGL");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");

			// base will set this to None
			PrecompileForTargets = PrecompileTargetsType.Default;
		}
	}
}
