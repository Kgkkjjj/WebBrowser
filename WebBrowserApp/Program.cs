using Microsoft.UI.Xaml;

namespace WebBrowserApp;

public class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        WinRT.ComWrappersSupport.InitializeComWrappers();
        var app = new App();
        app.InitializeComponent();
        app.Run();
    }
}
