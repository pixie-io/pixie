UPDATE api_keys
   SET unsalted_key = 'px-api-' || unsalted_key
 WHERE unsalted_key NOT LIKE 'px-api-%';
