using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine;

public static class BuildAudioPlayground
{
    public static void WindowsX64()
    {
        var output = Environment.GetEnvironmentVariable("OAE_PLAYGROUND_PLAYER_PATH");
        if (string.IsNullOrWhiteSpace(output) || !Path.IsPathFullyQualified(output))
            throw new BuildFailedException("Set OAE_PLAYGROUND_PLAYER_PATH to an absolute player path.");

        foreach (var bank in new[] { "playground.bank", "playground_vo.bank" })
        {
            if (!File.Exists(Path.Combine(Application.streamingAssetsPath, bank)))
                throw new BuildFailedException($"Missing {bank}. Run create_playground_assets.ps1 first.");
        }

        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var report = BuildPipeline.BuildPlayer(new BuildPlayerOptions
        {
            scenes = new[] { "Assets/Scenes/AudioPlayground.unity" },
            locationPathName = output,
            target = BuildTarget.StandaloneWindows64
        });
        if (report.summary.result != BuildResult.Succeeded)
            throw new BuildFailedException($"AudioPlayground build result: {report.summary.result}");
    }
}
