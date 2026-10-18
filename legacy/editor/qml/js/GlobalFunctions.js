// object name for display of correct icons 
function getIconName(string) {
    let name = ""
    if (string && string.length > 0) {
        name = string.charAt(1).toUpperCase() + string.slice(2, string.length - 1)
    }

    if (name.includes("materials_variants")) {
        name = "MaterialVariants"
    } else if (name.includes("primitive")) {
        name = "Primitive"
    } else if (name === "Meshe") {
        name = "Mesh"
    } else if (name.includes("light")) {
        name = "Light"
    } else if (name.includes("Extension")) {
        name = "Extension"
    } else if (name.includes("Animation")) {
        name = "Animation"
    }
    return name
}

// display the correct icon for the object 
function getObjIcon(string) {
    let icon = getIconName(string)
    if (icon && icon.length > 0) {
        return "qrc:/icons/Icon_Content_" + icon + ".svg"
    } else {
        return ""
    }
}

// get name to display e.g. "Node 1" from "/nodes/1" or "Primitive 2" from "/meshes/0/primitives/2"
function getDisplayName(name) {
    // replace all slashes with spaces
    const spaceName = name.slice(1).replace(/\//g, ' ');

    let lastSpaceIndex = spaceName.lastIndexOf(' ');
    const objectIndex = spaceName.slice(lastSpaceIndex);
    let objectName = spaceName.slice(0, lastSpaceIndex - 1);

    const lastSpaceIndexTwo = objectName.lastIndexOf(' ');
    if (lastSpaceIndexTwo != -1) {
        // extract stem e.g. "primitive 0 extensions KHR_materials_variants mapping" -> "mapping"
        objectName = objectName.slice(lastSpaceIndexTwo + 1);
    }

    if (objectName.includes("meshe")) {
        // turn e.g. "meshe 0" into "mesh 0"
        objectName = objectName.slice(0, objectName.length - 1);
    }

    if (objectName.includes("childre")) {
        // turn e.g. "childre 0" into "node 0"
        objectName = "node";
    }

    let displayName = objectName + objectIndex;
    // capitalize first letter
    return displayName.charAt(0).toUpperCase() + displayName.slice(1);
}

function getMaxSeverity(validationResults) {
    if (validationResults === undefined) {
        return -1;
    }
    let severity = 3;
    for (let issue of validationResults) {
        if (issue.severity < severity) {
            severity = issue.severity
        }
    }
    return severity
}

function getValidationBackgroundColor(defaultBackgroundColor, severity) {
    switch (severity) {
        case 0: return "#542E2E";
        case 1: return "#54523D";
        case 4:
        default: return defaultBackgroundColor;
    }
}

function getValidationTooltipColor(defaultTooltipColor, severity) {
    switch (severity) {
        case 0: return "#FF9999";
        case 1: return "#FFF266";
        case 4: return "#74a637";
        default: return defaultTooltipColor;
    }
}

function combineValidationText(severity, validationResults) {
    let message = ""
    if (validationResults === undefined || severity === -1) {
        return message
    }
    for (let issue of validationResults) {
        if (issue.severity === severity) {
            if (message !== "") {
                message += "\n"
            }
            message += issue.message
        }
    }
    return message
}

function getValidationIcon(severity) {
    switch (severity) {
        case 0: return "qrc:/icons/Icon_logger_alert_error.svg";
        case 1: return "qrc:/icons/Icon_logger_alert_warning.svg";
        case 2: return "qrc:/icons/Icon_logger_alert_information.svg";
        case 3:
        case 4: return "qrc:/icons/Icon_Success.svg"
        default: return "";
    }
}

function prettyPrintByteLength(length) {
    if (length < 1e3) {
        return length + " B";
    } else if (length < 1e6) {
        return (length / 1e3).toFixed(2) + " kB";
    } else if (length < 1e9) {
        return (length / 1e6).toFixed(2) + " MB";
    } else {
        return (length / 1e9).toFixed(2) + " GB";
    }
}