const OpenAI = require("openai");
const fs = require("fs").promises;
require('dotenv').config();

if (process.argv.length == 2) {
    console.error('Expected at least one argument!');
    process.exit(1);
}

const ipIndex = process.argv.indexOf('--ip');
let ipValue;

if (ipIndex > -1) {
    // Retrieve value after --ip
    ipValue = process.argv[ipIndex + 1];
}

const ip = (ipValue || 'http://localhost:4891/v1');

const pathIndex = process.argv.indexOf('-file');
let pathValue;

if (pathIndex > -1) {
    // Retreive value after -p for path to error json file
    pathValue = process.argv[pathIndex + 1];
}

const path = (pathValue || './Data/error_report.json');

console.log("Path to error_json file: ", `${path}`);
console.log("IP Address: ", `${ip}\n`)

let initialPromptTemplate = `Your task is to analyze a JSON object containing C++ file paths and lists of compiler errors. Each error includes lineNumber, columnNumber, errorDescription, and codeSnippet. Correct each error based on the errorDescription and return the corrections in a new JSON object with the same file path, including lineNumber, columnNumber, codeChangeDescription, and correctedCodeSnippet.

Guidelines:
- For 'expected ';' after [statement]', add a semicolon at the end of the statement.
- For 'use of undeclared identifier', if it's a function, declare it or include the correct header. If it's a misspelled variable, correct the spelling.
- For "Linker Error", do not modify anything and just return the object as is.

- Only return the JSON object with the corrections.
- Maintain the JSON format.

Example 1 Input:
{
    "./Code/automated/math_utils.cpp": [
        {
            "lineNumber": 3,
            "columnNumber": 21,
            "errorDescription": "expected ';' after return statement",
            "codeSnippet": [
                "double squareRoot(double number)",
                "{",
                "    return sqrt(number)",
                "}"
            ]
        }
    ],
    "./Code/automated/utility.cpp": [
        {
            "lineNumber": 4,
            "columnNumber": 5,
            "errorDescription": "use of undeclared identifier 'logMessage'",
            "codeSnippet": [
                "void debugPrint(int level)",
                "{",
                "    if (level > 2)",
                "        logMessage(\"Debug level high\")",
                "}"
            ]
        },
        {
            "lineNumber": 5,
            "columnNumber": 9,
            "errorDescription": "use of undeclared identifier 'coun'",
            "codeSnippet": [
                "int main()",
                "{",
                "    int count = 10;",
                "    for (int i = 0; i < count; i++)",
                "        coun << i << std::endl;",
                "}"
            ]
        }
    ]
}

Example 1 Output:
{
    "./Code/automated/math_utils.cpp": [
        {
            "lineNumber": 3,
            "columnNumber": 21,
            "codeChangeDescription": "Add a semicolon after the return statement.",
            "correctedCodeSnippet": [
                "double squareRoot(double number)",
                "{",
                "    return sqrt(number);",
                "}"
            ]
        }
    ],
    "./Code/automated/utility.cpp": [
        {
            "lineNumber": 4,
            "columnNumber": 5,
            "codeChangeDescription": "Declare 'logMessage' function or include its definition.",
            "correctedCodeSnippet": [
                "void logMessage(const std::string& msg);",
                "void debugPrint(int level)",
                "{",
                "    if (level > 2)",
                "        logMessage(\"Debug level high\");",
                "}"
            ]
        },
        {
            "lineNumber": 5,
            "columnNumber": 9,
            "codeChangeDescription": "Correct the spelling of 'count'.",
            "correctedCodeSnippet": [
                "int main()",
                "{",
                "    int count = 10;",
                "    for (int i = 0; i < count; i++)",
                "        count << i << std::endl;",
                "}"
            ]
        }
    ]
}

Example 2 Input:
{
  "Linker Error": [
      {
          "lineNumber": 0,
          "columnNumber": 0,
          "errorDescription": "ld: Undefined symbols:\n  subtract(int, int), referenced from:\n      _main in calculator-0e2fd2.o\nlinker command failed with exit code 1 (use -v to see invocation)"
      }
  ]
}

Example 2 Output:
{
  "Linker Error": [
      {
          "lineNumber": 0,
          "columnNumber": 0,
          "errorDescription": "ld: Undefined symbols:\n  subtract(int, int), referenced from:\n      _main in calculator-0e2fd2.o\nlinker command failed with exit code 1 (use -v to see invocation)"
      }
  ]
}

End of Examples

Your Task:
`;


async function readJsonFile(path) {
  try {
    const data = await fs.readFile(path, 'utf8');
    return JSON.parse(data);
  } catch (err) {
    console.error("Error reading or parsing JSON file", err);
    throw err;
  }
}

async function generateInitialPrompt() {
  try {
    let json = await readJsonFile(path);
    // Remove already corrected errors
    json = removeCorrectedErrors(json, correctionHistory);
    const jsonString = JSON.stringify(json, null, 4);

    let prompt = initialPromptTemplate + jsonString;
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
        "content": "You are a helpful assistant who is good at identifying and correcting C++ compiler errors, and you are designed to output JSON."
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

const historyFilePath = './Data/correction_history.json';

// Function to read history from a file
async function readHistoryFile() {
    try {
        const data = await fs.readFile(historyFilePath, 'utf8');
        return JSON.parse(data);
    } catch (err) {
        // If the file doesn't exist, return an empty array
        return [];
    }
}

// Function to write history to a file
async function writeHistoryFile(history) {
    try {
        const data = JSON.stringify(history, null, 4);
        await fs.writeFile(historyFilePath, data);
    } catch (err) {
        console.error("Error writing history to file", err);
        throw err;
    }
}

// Global variable to store history of corrections
let correctionHistory = [];

// Function to update history with new corrections
function updateHistoryWithCorrections(json, history) {
  for (const file in json) {
      json[file].forEach(error => {
          const errorSignature = `${file}:${error.lineNumber}:${error.columnNumber}`;
          const suggestion = error.codeChangeDescription || "No suggestion available";
          const historyEntry = { errorSignature, suggestion };

          // Check if this exact error has already been corrected
          const isAlreadyCorrected = history.some(entry => 
            entry.errorSignature === errorSignature
          );

          if (!isAlreadyCorrected) {
            history.push(historyEntry);
          }
      });
  }
}

// Function to remove already corrected errors
function removeCorrectedErrors(json, history) {
  for (const file in json) {
      json[file] = json[file].filter(error => {
          const errorSignature = `${file}:${error.lineNumber}:${error.columnNumber}`;
          return !history.includes(errorSignature);
      });
  }
  return json;
}

// Main async function to orchestrate the operations
async function main() {
  try {
    // Read history from file
    correctionHistory = await readHistoryFile();

    const prompt = await generateInitialPrompt();
    const response = await callOpenAI(prompt);
    await writeResponseToFile(response, './Data/corrected_code.json');

    // Update history with new corrections
    updateHistoryWithCorrections(JSON.parse(response), correctionHistory);

    // Write updated history to file
    await writeHistoryFile(correctionHistory);
  } catch (err) {
    console.error("An error occurred:", err);
  }
}

main();