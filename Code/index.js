const OpenAI = require("openai");
const express = require("express");
const fs = require("fs").promises;

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

let initialPromptTemplate = `Analyze a JSON object containing C++ file paths and lists of compiler errors. Each error includes lineNumber, columnNumber, errorDescription, and codeSnippet. Correct each error based on the errorDescription and return the corrections in a new JSON object with the same file path, including lineNumber, columnNumber, codeChangeDescription, and correctedCodeSnippet.

Guidelines:
- For 'expected ';' after [statement]', add a semicolon at the end of the statement.
- For 'use of undeclared identifier', if it's a function, declare it or include the correct header. If it's a misspelled variable, correct the spelling.

- Only return the JSON object with the corrections.
- Maintain the JSON format.

Example Input:
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

Example Output:
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
    const json = await readJsonFile(path);
    const jsonString = JSON.stringify(json, null, 4);

    let prompt = initialPromptTemplate + jsonString;
    return prompt;
  } catch (err) {
    console.error("Failed to generate prompt:", err);
    return null;
  }
}

// Function for appending new errors to the prompt template
function generatePromptWithNewErrors(newErrors) {
  const newErrorsString = JSON.stringify(newErrors, null, 4);
  return initialPromptTemplate + newErrorsString;
}

const openai = new OpenAI({
  baseURL: ip,
  apiKey : "not needed for a local LLM",
});

// LLM Models
const minstralInstructModel = "mistral-7b-instruct-v0.1.Q4_0.gguf";
const minstralOpenOrcaModel = "mistral-7b-openorca.Q4_0.gguf";


let repsonseQueue = [];
const MAX_RESPONSES = 10;

const app = express();
const port = 3000;

app.use(express.json());

// Check if server is up and running
app.get("/health", (req, res) => {
  res.send("GPT4All REST Server says hello world!");
});

// POST request to prompt GPT4All and receive response
app.post('/gpt', async (req, res) => {
  try {

    const newErrors = req.body.newErrors;
    let prompt;

    if(newErrors) {
      // Generate prompt with new errors for subsequent requests
      prompt = generatePromptWithNewErrors(newErrors);
    } else {
      // Use the initial prompt for the first request
      prompt = await generateInitialPrompt();
    }

    const {data: chatCompletion, response: raw } = await openai.chat.completions.create({
      messages: [{role: req.params.role, content: prompt }],
      model: minstralOpenOrcaModel,
      max_tokens: 4000,
   }).withResponse();

   const responseKey = Date.now().toString();
  
   // Check if the queue of responses is full, and remove oldest item
  if(repsonseQueue.length >= MAX_RESPONSES) {
    repsonseQueue.shift();
  }

  // Add a new response to the queue
  chatCompletion.choices.forEach(choice => {
    repsonseQueue.push({  key: responseKey, gptResponse: choice.message.content  })
  });

  res.json({ message: "Response stored", key: responseKey, chatContents: chatCompletion.choices.map(choice => choice.message.content) });
  } catch (error) {
    res.status(500).send(error.toString());
  }
});

// GET request to fetch a single response based on key
app.get('/response/:key', (req, res) => {
  const responseKey = req.params.key;
  const response = repsonseQueue.find(item => item.key === responseKey);

  if (response) {
    res.json(response);
  } else {
    res.status(404).send('Response not found');
  }
});

app.listen(port, () => {
  console.log(`REST server listening on port ${port}`);
})

async function applyFixesToSourceCode(llmResponse) {
  for (const filePath in llmResponse) {
    let fileContent = await fs.readFile(filePath, 'utf-8');
    let fileLines = fileContent.split('\n');

    for (const change of llmResponse[filePath]) {
      const { lineNumber, correctedCodeSnippet } = change;
      // Apply the change - replace the line and the following lines as needed
      fileLines.splice(lineNumber - 1, correctedCodeSnippet.length, ...correctedCodeSnippet);
    }

    // Join the modified lines and write back to the file
    const newContent = fileLines.join('\n');
    await fs.writeFile(filePath, newContent);
  }
}