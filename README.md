# WebBrowserApp

This sample demonstrates a minimal WinUI 3 browser built with C#. It uses
`WebView2` for web browsing and shows how to call a free AI model through the
HuggingFace Inference API. The project requires the Windows App SDK and
.NET 7 to build and run.

## Building

1. Install the [Windows App SDK](https://learn.microsoft.com/windows/apps/windows-app-sdk/) and .NET 7 SDK.
2. Navigate to the `WebBrowserApp` folder.
3. Run `dotnet build` to restore packages and build the project.

## Running

After building, you can run the app with `dotnet run --project WebBrowserApp.csproj`.
Enter any URL to navigate. The `AskModelAsync` method shows how you could send
text prompts to an AI model for processing via the HuggingFace API.
