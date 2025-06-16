using Microsoft.UI.Xaml;

namespace WebBrowserApp;

public partial class App : Application
{
    public App()
    {
        this.InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var setup = new SetupWindow();
        setup.Activate();
        setup.Closed += (_, _) =>
        {
            var window = new MainWindow();
            window.Activate();
        };
    }
}
