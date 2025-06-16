using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.Web.WebView2.Core;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

namespace WebBrowserApp;

public sealed partial class MainWindow : Window
{
    private readonly HttpClient _httpClient = new();

    public MainWindow()
    {
        this.InitializeComponent();
    }

    private void OnGoClicked(object sender, RoutedEventArgs e)
    {
        var address = AddressBox.Text;
        if (!string.IsNullOrWhiteSpace(address))
        {
            if (!address.StartsWith("http"))
                address = "https://" + address;
            Browser.Source = new Uri(address);
        }
    }

    private async Task<string> AskModelAsync(string prompt)
    {
        var requestBody = new { inputs = prompt };
        var content = new StringContent(JsonSerializer.Serialize(requestBody), System.Text.Encoding.UTF8, "application/json");
        // Example uses HuggingFace public inference API for a free model
        var response = await _httpClient.PostAsync("https://api-inference.huggingface.co/models/gpt2", content);
        if (response.IsSuccessStatusCode)
        {
            var json = await response.Content.ReadAsStringAsync();
            return json;
        }
        return "";
    }
}
