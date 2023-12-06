const fs = require('fs');

const correctedJsonFilePath = "./Data/corrected_code.json";

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
    
    for (const filePath in corrections) {
        if (fs.existsSync(filePath)) {
        console.log(`Current file path: ${filePath}`);

        let fileData = fs.readFileSync(filePath, 'utf8');
        let lines = fileData.split('\n');

        corrections[filePath].forEach(change => {
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

    console.log('Corrections applied.');
}

applyCorrections();
