const https = require('https');

const apis = {
  'Public APIs': 'https://api.publicapis.org/entries',
  'Random Dog': 'https://dog.ceo/api/breeds/image/random',
  'Cat Fact': 'https://catfact.ninja/fact',
  'Agify': 'https://api.agify.io?name=John',
  'Genderize': 'https://api.genderize.io?name=John',
  'Nationalize': 'https://api.nationalize.io?name=John',
  'Joke': 'https://official-joke-api.appspot.com/jokes/random',
  'Bored': 'https://www.boredapi.com/api/activity',
  'Bitcoin Price': 'https://api.coindesk.com/v1/bpi/currentprice.json',
  'IPify': 'https://api.ipify.org?format=json'
};

for (const [name, url] of Object.entries(apis)) {
  https.get(url, (res) => {
    let data = '';
    res.on('data', (chunk) => data += chunk);
    res.on('end', () => {
      console.log('--- ' + name + ' ---');
      try {
        console.log(JSON.parse(data));
      } catch (err) {
        console.error('Error parsing ' + name + ':', err.message);
      }
    });
  }).on('error', (err) => {
    console.error('Request error for ' + name + ':', err.message);
  });
}
