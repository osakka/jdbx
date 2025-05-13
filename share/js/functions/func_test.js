// Test function in the functions directory
function userFunction(args) {
    console.log("Successfully loaded func_test.js from functions directory");
    return {
        message: "Success from functions directory",
        args: args
    };
}