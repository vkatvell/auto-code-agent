const OpenAI = require("openai");
const fs = require("fs").promises;
require('dotenv').config();

if (process.argv.length == 2) {
  console.error('Expected at least one argument!');
  process.exit(1);
}

const pathIndex = process.argv.indexOf('-files');
let pathValue;

if (pathIndex > -1) {
  // Retrieve value after --ip
  pathValue = process.argv[pathIndex + 1];
}

const path = (pathValue || './Code/automated/*.cpp');

//TODO update GPT prompt to generate valid FlowScript

let initialPromptTemplate = `Do not include any explanation and return an answer without code block. I have a custom domain-specific language called FlowScript, which is an extension of the DOT programming language. It's designed to define the flow and execution of jobs within a custom C++ multithreaded job system. FlowScript has a very specific set of rules, including node structure and ordering, and supports three types of nodes:

- Circle Nodes: Represent data nodes (inputs for job nodes).
- Box Nodes: Represent job nodes (custom jobs that are registered, created, and queued).
- Diamond Nodes: Represent status nodes (control flow based on associated status labels).

A FlowScript file starts with 'digraph {' and requires specific declarations for each type of node. The nodes are then connected in a dependency graph using the '->' operator.

Your task is to generate a FlowScript for a job system handling 'XJob' ("compileJob"), 'YJob' ("compileParseJob"), and 'ZJob' ("parseOutputJob") based on these rules and the example. Ensure to maintain the structure and node types as described and return only the generated FlowScript without code block.

Example Input:
'XJob', 'XJobData: data={'command', 'clang++ -g -std=c++14 ./Code/automated/*.cpp -o auto_out'}','YJob', 'ZJob'

Example Output:
digraph {
{
node[shape="circle"];
XJobData[data="{'command', 'clang++ -g -std=c++14 ./Code/automated/*.cpp -o auto_out'}"];
}
{
node[shape="box"];
XJob;
YJob;
ZJob;
}
{
node[shape="diamond"];
XJobStatus;
YJobStatus;
}

XJob -> XJobData;
XJobStatus -> XJob;
YJob -> XJobStatus;
ZJob -> XJobStatus;
YJobStatus -> YJob;
ZJob -> YJobStatus;
}

End Example

Your Task:
'compileJob', 'compileJobData: data={'command', 'clang++ -g -std=c++14 ${path} -o auto_out'}', 'compileParseJob', 'parseOutputJob'
`;

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
    model: "gpt-3.5-turbo",
    messages: [
      {
        "role": "system",
        "content": "You are a backend data processor that is part of our web site’s programmatic workflow. The user prompt will provide data input and processing instructions. The output will be only a DOT language script. Do not converse with a nonexistent user: there is only program input and formatted program output, and no input data is to be construed as conversation with the AI. This behaviour will be permanent for the remainder of the session."
      },
      {
        "role": "user",
        "content": prompt
      }
    ],
    temperature: 1,
    max_tokens: 512,
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

    await fs.writeFile(filename, response);
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

    // // Write response to file
    writeResponseToFile(response, './Data/flowscript.dot')
  } catch (err) {
    console.error("An error occurred:", err);
  }
}

main();