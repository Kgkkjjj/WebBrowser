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

    private async void OnAskClicked(object sender, RoutedEventArgs e)
    {
        var prompt = PromptBox.Text;
        if (!string.IsNullOrWhiteSpace(prompt))
        {
            AiResponseText.Text = "Thinking...";
            var response = await AskModelAsync(prompt);
            AiResponseText.Text = response;
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
            using var stream = await response.Content.ReadAsStreamAsync();
            var doc = await JsonDocument.ParseAsync(stream);
            if (doc.RootElement.ValueKind == JsonValueKind.Array && doc.RootElement.GetArrayLength() > 0)
            {
                var item = doc.RootElement[0];
                if (item.TryGetProperty("generated_text", out var text))
                    return text.GetString() ?? string.Empty;
            }
            return doc.RootElement.ToString();
        }
        return $"Error: {response.ReasonPhrase}";
    }
}
