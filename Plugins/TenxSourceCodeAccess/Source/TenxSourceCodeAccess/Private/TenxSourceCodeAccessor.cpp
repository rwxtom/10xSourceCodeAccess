//Copyright (c) 2022 rwxtom
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#include "TenxSourceCodeAccessor.h"
#include "Windows/WindowsPlatformMisc.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"
#include "ISourceCodeAccessModule.h"

#define LOCTEXT_NAMESPACE "FTenxSourceCodeAccessor"

DECLARE_LOG_CATEGORY_EXTERN(LogTenxSourceCodeAccessor, Log, All);
DEFINE_LOG_CATEGORY(LogTenxSourceCodeAccessor);

void FTenxSourceCodeAccessor::RefreshAvailability()
{
	TenxLocation = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("TENX_EXECUTABLE"));
	bHasTenxInstalled = FPaths::FileExists(TenxLocation);

	if (!bHasTenxInstalled)
	{
		UE_LOG(LogTenxSourceCodeAccessor, Error, TEXT("Unable to find TENX_EXECUTABLE environment variable."));
		return;
	}
}

bool FTenxSourceCodeAccessor::CanAccessSourceCode() const
{
	return bHasTenxInstalled;
}

FName FTenxSourceCodeAccessor::GetFName() const
{
	return FName("10x");
}

FText FTenxSourceCodeAccessor::GetNameText() const
{
	return LOCTEXT("10xDisplayName", "10x");
}

FText FTenxSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("10xDisplayDesc", "Open source files in 10x");
}

bool FTenxSourceCodeAccessor::OpenSolution()
{
	if (!bHasTenxInstalled)
		return false;

	const FString ProjectFileLocation = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString ProjectDirectory = FPaths::GetPath(ProjectFileLocation);

	return OpenSolutionAtPath(ProjectDirectory);
}

bool FTenxSourceCodeAccessor::OpenSolutionAtPath(const FString& InSolutionPath)
{
	if (!bHasTenxInstalled)
		return false;

	// We can safely assume that .uproject file and .sln file are in project
	// directory, so we can just change the file extension..
	FString UProj = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString Left, Right;
	UProj.Split(TEXT("."), &Left, &Right);
	const FString Path = Left += TEXT(".sln");

	const FString Params = FString::Printf(TEXT("\"%s\""), *Path);

	return RunTenx([this, &Params, &Path]()->bool
	{
		FProcHandle Proc = FPlatformProcess::CreateProc(*TenxLocation, *Params, true, true, false, nullptr, 0, nullptr, nullptr);
		const bool bSuccess = Proc.IsValid();
		if (!bSuccess)
		{
			UE_LOG(LogTenxSourceCodeAccessor, Warning, TEXT("Failed to open project file %s."), *Path);
			FPlatformProcess::CloseProc(Proc);
			return false;
		}
		return bSuccess;
	});
}

bool FTenxSourceCodeAccessor::RunTenx(const TFunction<bool()> Callback) const
{
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>(TEXT("SourceCodeAccess"));
	SourceCodeAccessModule.OnLaunchingCodeAccessor().Broadcast();
	const bool bSuccess = Callback();
	SourceCodeAccessModule.OnDoneLaunchingCodeAccessor().Broadcast(bSuccess);
	return bSuccess;
}

bool FTenxSourceCodeAccessor::DoesSolutionExist() const
{
	const FString SolutionLocation = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	return FPaths::FileExists(SolutionLocation);
}

bool FTenxSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	if (!bHasTenxInstalled)
		return false;

	const FString Path = FString::Printf(TEXT("\"%s\""), *FullPath);
	const FString Params = FString::Printf(TEXT("%s N10X.Editor.SetCursorPos((0,%d))"), *Path, LineNumber - 1);

	return RunTenx([this, &Params, &Path]()->bool
		{
			FProcHandle Proc = FPlatformProcess::CreateProc(*TenxLocation, *Params, true, true, false, nullptr, 0, nullptr, nullptr);
			const bool bSuccess = Proc.IsValid();
			if (!bSuccess)
			{
				UE_LOG(LogTenxSourceCodeAccessor, Warning, TEXT("Failed to open source files %s"), *Path);
				FPlatformProcess::CloseProc(Proc);
				return false;
			}
			return bSuccess;
		});
}

bool FTenxSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths)
{
	if (!bHasTenxInstalled)
		return false;

	FString SolutionPath = FPaths::GetProjectFilePath();

	FString SourceLocations = "";
	for (const FString& SourcePath : AbsoluteSourcePaths)
	{
		SourceLocations += FString::Printf(TEXT("\"%s\" "), *SourcePath);
	}
	
	const FString Params = FString::Printf(TEXT("\"%s\" %s"), *SolutionPath, *SourceLocations);

	return RunTenx([this, &Params, &SourceLocations]()->bool
	{
		FProcHandle Proc = FPlatformProcess::CreateProc(*TenxLocation, *Params, true, true, false, nullptr, 0, nullptr, nullptr);
		const bool bSuccess = Proc.IsValid();
		if (!bSuccess)
		{
			UE_LOG(LogTenxSourceCodeAccessor, Warning, TEXT("Failed to open source files %s"), *SourceLocations);
			FPlatformProcess::CloseProc(Proc);
			return false;
		}
		return bSuccess;
	});
}

bool FTenxSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	return true;
}

bool FTenxSourceCodeAccessor::SaveAllOpenDocuments() const
{
	return false;
}

void FTenxSourceCodeAccessor::Tick(const float DeltaTime){}

#undef LOCTEXT_NAMESPACE