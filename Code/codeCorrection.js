const fs = require('fs');

const errorReportJsonFilePath = "./Data/error_report.json";
const correctedJsonFilePath = "./Data/corrected_code.json";
const descriptionFilePath = "./Data/code_change_descriptions.txt";

// Reads JSON data from a file
function readJsonFile(filePath) {
    const data = fs.readFileSync(filePath, 'utf8');
    return JSON.parse(data);
}

// Writes data to a file
function writeFile(filePath, data) {
    fs.writeFileSync(filePath, data, 'utf8');
}

// Main function to apply corrections
function applyCorrections() {
    const corrections = readJsonFile(correctedJsonFilePath);
    const errorReport = readJsonFile(errorReportJsonFilePath);

    // String to store code change descriptions 
    let descriptionData = "";

    if (errorReport["Linker Error"]) {
        console.log("Linker Error Detected:");
        errorReport["Linker Error"].forEach(error => {
            console.log(error.errorDescription);
            // Append the linker error description to the descriptionData string
            descriptionData += `Linker Error: ${error.errorDescription}\n`;
        });
        // Write the linker error description to the file and stop further processing
        writeFile(descriptionFilePath, descriptionData);
        return;
    }

    for (const filePath in corrections) {
        if (fs.existsSync(filePath)) {
        console.log(`Current file path: ${filePath}`);

        let fileData = fs.readFileSync(filePath, 'utf8');
        let lines = fileData.split('\n');

        corrections[filePath].forEach(change => {
            // Append description to the descriptionData string
            descriptionData += `File: ${filePath}, Line: ${change.lineNumber}, Column: ${change.columnNumber}, Description: ${change.codeChangeDescription}\n`;

            // Adjusting the line number to point to the actual line with the error
            const errorLineNumber = change.lineNumber - 1; // 0-based index for the line with the error
            const snippetStartLineNumber = errorLineNumber - 2; // Start of the snippet in the JSON object

            const correctedSnippet = change.correctedCodeSnippet;

            // Replace the lines in the source file with the corrected snippet lines
            for (let i = 0; i < correctedSnippet.length; i++) {
                lines[snippetStartLineNumber + i] = correctedSnippet[i];
            }
        });

        fileData = lines.join('\n');
        writeFile(filePath, fileData);
            
        } else {
            console.error(`File not found: ${filePath}`);
        }
        
    }

    // Write the collected descriptions to a separate file
    writeFile(descriptionFilePath, descriptionData);

    console.log('Corrections applied.');
}

applyCorrections();
