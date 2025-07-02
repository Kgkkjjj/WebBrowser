import os
from datasets import Dataset
from transformers import (
    AutoTokenizer,
    AutoModelForCausalLM,
    GPT2Config,
    Trainer,
    TrainingArguments,
)

# Simple chain-of-thought style dataset
COMPLEX_DATA = [
    {
        "question": "John has 3 apples. He buys 2 more and gives 1 to Mary. How many apples does John have now?",
        "answer": "John starts with 3 apples. After buying 2 more he has 5. Giving 1 to Mary leaves him with 4 apples.",
    },
    {
        "question": "A store sold 5 pencils on Monday and twice as many on Tuesday. How many pencils were sold in total?",
        "answer": "The store sold 5 pencils on Monday. Twice as many on Tuesday is 10. In total, 5 + 10 = 15 pencils were sold.",
    },
    {
        "question": "Sarah read 4 pages of a book each day for 3 days and then 2 pages on the fourth day. How many pages did she read in all?",
        "answer": "She read 4 pages a day for 3 days, so 4 * 3 = 12 pages. Adding the 2 pages on the fourth day gives 12 + 2 = 14 pages.",
    },
    {
        "question": "There are 8 cars in a parking lot. If 3 more arrive and 2 leave, how many cars are in the lot?",
        "answer": "Starting with 8 cars, adding 3 brings it to 11, and removing 2 results in 9 cars left in the lot.",
    },
    {
        "question": "A recipe calls for 2 cups of flour. If you double the recipe and then spill half a cup, how much flour do you use?",
        "answer": "Doubling the recipe means 4 cups of flour. Spilling half a cup leaves 4 - 0.5 = 3.5 cups.",
    },
]

MODEL_NAME = "gpt2"
OUTPUT_DIR = "./complex_model_output"
MAX_LENGTH = 512
NUM_LAYERS = 30


def preprocess(example, tokenizer):
    text = f"Question: {example['question']}\nAnswer: {example['answer']}"
    tokens = tokenizer(text, truncation=True, max_length=MAX_LENGTH)
    tokens["labels"] = tokens["input_ids"].copy()
    return tokens


def main():
    tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)

    config = GPT2Config.from_pretrained(MODEL_NAME)
    config.n_layer = NUM_LAYERS
    model = AutoModelForCausalLM.from_config(config)

    dataset = Dataset.from_list(COMPLEX_DATA)
    dataset = dataset.map(lambda x: preprocess(x, tokenizer), remove_columns=["question", "answer"])

    training_args = TrainingArguments(
        output_dir=OUTPUT_DIR,
        per_device_train_batch_size=2,
        num_train_epochs=3,
        logging_steps=10,
        save_steps=50,
        max_steps=200,
    )

    trainer = Trainer(model=model, args=training_args, train_dataset=dataset)
    trainer.train()

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    trainer.save_model(OUTPUT_DIR)


if __name__ == "__main__":
    main()
