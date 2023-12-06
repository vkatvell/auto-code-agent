const fs = require('fs');

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
    const corrections = readJsonFile('corrected_code.json');

    corrections.forEach(correction => {
        for (const filePath in correction) {
            let fileData = fs.readFileSync(filePath, 'utf8');
            const lines = fileData.split('\n');

            correction[filePath].forEach(change => {
                const lineNumber = change.lineNumber - 1; // Array is 0-indexed
                lines[lineNumber] = change.correctedCodeSnippet.join('\n');
            });

            fileData = lines.join('\n');
            writeFile(filePath, fileData);
        }
    });

    console.log('Corrections applied.');
}

applyCorrections();
