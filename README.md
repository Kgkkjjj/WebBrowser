# GPT-2 Thinking Model

This repository provides a simple example of how to fine-tune GPT-2 on a reasoning-focused dataset. The example uses the [Hugging Face Transformers](https://github.com/huggingface/transformers) library and the `gsm8k` dataset, which contains grade-school level math word problems.

## Requirements

- Python 3.8+
- `transformers` library
- `datasets` library
- `torch`

You can install the dependencies with:

```bash
pip install transformers datasets torch
```

## Training

The script `train_gpt2_thinking_model.py` demonstrates how to fine-tune GPT-2 on the `gsm8k` dataset. To run the training with default parameters:

```bash
python train_gpt2_thinking_model.py
```

The script downloads the dataset automatically. Training results and the model will be saved in the `./model_output` directory.

The model is initialized with a custom GPT-2 configuration that uses **30 transformer layers** to encourage more complex reasoning.

## Complex Thinking Example

For a more advanced demonstration, the script `train_complex_thinking_model.py`
uses a small chain-of-thought style dataset defined directly in the code. It
also initializes GPT-2 with 30 layers and trains for a few epochs.

Run it with:

```bash
python train_complex_thinking_model.py
```

Training outputs are written to `./complex_model_output`.

## Notes

This example uses a small subset of the dataset by default for faster training. Adjust the hyperparameters in the script to experiment with different configurations or larger datasets.
