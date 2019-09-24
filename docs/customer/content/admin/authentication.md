---
title: "Authentication"
metaTitle: "Administration | Pixie"
metaDescription: "How to Install ..."
---

### User Accounts 

Pixie uses [Auth0](https://auth0.com/docs) to manage end-user authorization and to integrate with identity providers. We support the following providers:

| Identity Provider     | Support       | Notes                                            |
| :---------------------| :------------ | :--------------------------------------------    |
| Google                | Supported     | We do not support personal `@gmail.com` emails   |
| Github                | Not Supported | Planned in Beta roadmap                          |
| Active Directory      | Not Supported | Not in roadmap                                   |


### Web Certificate Verification

During deployment of Alpha and Beta releases, Admins will be required to self-certify the Pixie Console web application in their browser after they successfully deploy Pixie in their Kubernetes cluster. 

The self-verification will include the following steps: 
- Open your browsers' developer tools console.
- Locate errors related to accessing the URL: `<IP>/graphql`.
- Open the URL in a new tab, which should ask you to validate the certificates.
- Validate the certificate and close the tab. 
- The Pixie Console page should then be fully functional. 

**Note:** Self-certification via the browser console is not the ideal experience. We are actively working on implementing a solution that will not need self-certification.