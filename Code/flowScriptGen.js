const OpenAI = require("openai");
const fs = require("fs").promises;
require('dotenv').config();

//TODO update GPT prompt to generate valid FlowScript

let initialPromptTemplate = ``;

async function generateInitialPrompt() {
  try {
    let prompt = initialPromptTemplate;
    return prompt;
  } catch (err) {
    console.error("Failed to generate prompt:", err);
    return null;
  }
}

const openai = new OpenAI({
  apiKey : process.env.OPENAI_API_KEY,
});


async function callOpenAI(prompt) {
  const completion = await openai.chat.completions.create({
    model: "gpt-3.5-turbo-1106",
    response_format: { type: "json_object" },
    messages: [
      {
        "role": "system",
        "content": "You are a helpful assistant."
      },
      {
        "role": "user",
        "content": prompt
      }
    ],
    temperature: 0.8,
    max_tokens: 1024,
    top_p: 1,
    frequency_penalty: 0,
    presence_penalty: 0,
  });

  // Return the response
  console.log(completion.choices[0].message.content)
  return completion.choices[0].message.content;
}

// Function to write response to a file
async function writeResponseToFile(response, filename) {
  try {
    if (!response) {
      throw new Error("Response is undefined or null");
    }

    let dataToWrite;
    if (typeof response === 'string') {
      // If response is a string, parse it as JSON
      dataToWrite = JSON.parse(response);
    } else {
      // If response is already a JSON object
      dataToWrite = response;
    }

    const formattedJson = JSON.stringify(dataToWrite, null, 4);
    await fs.writeFile(filename, formattedJson);
    console.log(`Response written to file: ${filename}`);
  } catch (err) {
    console.error("Error writing response to file", err);
    throw err;
  }
}

// Main async function to orchestrate the operations
async function main() {
  try {

    const prompt = await generateInitialPrompt();
    const response = await callOpenAI(prompt);

    // Write response to file
    writeResponseToFile(response, './Data/fstest.dot')
  } catch (err) {
    console.error("An error occurred:", err);
  }
}

main();