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
        let fileData = fs.readFileSync(filePath, 'utf8');
        let lines = fileData.split('\n');

        corrections[filePath].forEach(change => {
            const lineNumber = change.lineNumber - 1; // 0-based index
            const columnNumber = change.columnNumber - 1; // 0-based index

            const originalLine = lines[lineNumber];
            const correctedLine = change.correctedCodeSnippet.join('\n');
            
            // Replace the specific part of the line based on the column number
            if (originalLine.trim() !== correctedLine.trim()) {
                const beforeError = originalLine.substring(0, columnNumber);
                const afterError = originalLine.substring(columnNumber);

                // Find the position in the corrected line to start the replacement
                const replacementStartIndex = correctedLine.indexOf(afterError);
                const replacement = correctedLine.substring(0, replacementStartIndex);

                lines[lineNumber] = beforeError + replacement + afterError;
            } else {
                // If the entire line is a corrected snippet, replace the line
                lines[lineNumber] = correctedLine;
            }
        });

        fileData = lines.join('\n');
        writeFile(filePath, fileData);
    }

    console.log('Corrections applied.');
}

applyCorrections();
