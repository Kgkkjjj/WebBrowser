# WebBrowserApp

This sample demonstrates a minimal WinUI 3 browser built with C#. It uses
`WebView2` for web browsing and shows how to call a free AI model through the
HuggingFace Inference API. The project requires the Windows App SDK and
.NET 7 to build and run.

When launched, the app displays a simple setup window that can automatically
restore NuGet packages and build the project. The browser UI includes a prompt
box so you can send text to the example AI model and see the response in the
window.

## Building

1. Install the [Windows App SDK](https://learn.microsoft.com/windows/apps/windows-app-sdk/) and .NET 7 SDK.
2. Navigate to the `WebBrowserApp` folder.
3. Run `dotnet build` to restore packages and build the project. You can also
   rely on the in-app setup screen which performs these steps for you.

## Running

After building, you can run the app with `dotnet run --project WebBrowserApp.csproj`.
Enter any URL to navigate. Below the browser area is a text box and button for
sending prompts to the HuggingFace API. Responses are displayed directly in the
window so you can experiment with the model without leaving the app.

## Setup Screen

When the application launches, a setup window appears. Choosing **Install Packages**
runs `dotnet restore` and `dotnet build`. Once setup finishes, the main browser
window opens automatically.
