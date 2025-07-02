import os
from datasets import load_dataset
from transformers import (
    AutoTokenizer,
    AutoModelForCausalLM,
    GPT2Config,
    Trainer,
    TrainingArguments,
)

# Use a small subset of the GSM8K dataset for demonstration
DATASET_NAME = "gsm8k"
DATASET_CONFIG = "main"
SPLIT = "train[:1%]"
MODEL_NAME = "gpt2"
OUTPUT_DIR = "./model_output"
MAX_LENGTH = 512
NUM_LAYERS = 30


def preprocess(example, tokenizer):
    # Combine question and answer into a single string
    prompt = example["question"].strip()
    answer = example["answer"].strip()
    text = f"Question: {prompt}\nAnswer: {answer}"
    tokens = tokenizer(text, truncation=True, max_length=MAX_LENGTH)
    tokens["labels"] = tokens["input_ids"].copy()
    return tokens


def main():
    tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)

    config = GPT2Config.from_pretrained(MODEL_NAME)
    config.n_layer = NUM_LAYERS
    model = AutoModelForCausalLM.from_config(config)

    dataset = load_dataset(DATASET_NAME, DATASET_CONFIG, split=SPLIT)
    dataset = dataset.map(lambda x: preprocess(x, tokenizer), remove_columns=dataset.column_names)

    training_args = TrainingArguments(
        output_dir=OUTPUT_DIR,
        per_device_train_batch_size=2,
        num_train_epochs=1,
        logging_steps=10,
        save_steps=50,
        max_steps=100,
    )

    trainer = Trainer(model=model, args=training_args, train_dataset=dataset)
    trainer.train()

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    trainer.save_model(OUTPUT_DIR)


if __name__ == "__main__":
    main()
