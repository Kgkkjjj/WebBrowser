using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Diagnostics;

using System.Threading.Tasks;

namespace WebBrowserApp;

public sealed partial class SetupWindow : Window
{
    public SetupWindow()
    {
        this.InitializeComponent();
    }

    private async void OnInstallClicked(object sender, RoutedEventArgs e)
    {
        InstallButton.IsEnabled = false;
        StatusText.Text = "Restoring packages...";
        await RunProcessAsync("dotnet", "restore");
        StatusText.Text = "Building application...";
        await RunProcessAsync("dotnet", "build --configuration Release");
        StatusText.Text = "Setup complete";
        InstallButton.IsEnabled = true;
        Close();
    }

    private static Task RunProcessAsync(string fileName, string arguments)
    {
        var tcs = new TaskCompletionSource<bool>();
        var process = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                CreateNoWindow = true,
                UseShellExecute = false
            },
            EnableRaisingEvents = true
        };
        process.Exited += (s, e) => tcs.SetResult();
        process.Start();
        return tcs.Task;
    }
}
