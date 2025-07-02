# GPT-2 Thinking Model

This repository demonstrates how to fine-tune GPT-2 on a small reasoning-oriented dataset. It relies on the [Hugging Face Transformers](https://github.com/huggingface/transformers) library and the `gsm8k` dataset, which contains grade-school math word problems.

## Requirements

- Python 3.8+
- `transformers` library
- `datasets` library
- `torch`

Install dependencies with:

```bash
pip install transformers datasets torch
```

## Training

The script `train_thinking_model.py` trains GPT-2 using a combination of a small subset of the `gsm8k` dataset and several chain-of-thought style examples defined directly in the code.

Run training with:

```bash
python train_thinking_model.py
```

Training results and the model will be saved in `./thinking_model_output`.

The model is initialized with a custom configuration that uses **30 transformer layers** to encourage deeper reasoning.

## Notes

By default only a tiny portion of the dataset is used so the example runs quickly. Adjust the parameters in the script if you want to experiment with longer training or larger datasets.
