# Ory Hydra and Kratos Upgrade Notes

## Version Changes
- **Hydra**: v1.9.2 → v2.2.0
- **Kratos**: v0.10.1 → v1.2.0

## CRITICAL: Pre-Upgrade Requirements

### 1. Database Backup
**MANDATORY**: Create a full database backup before upgrading. Both Hydra v2 and Kratos v1 require SQL migrations that may cause data loss if not handled properly.

### 2. Test Environment
Test the upgrade process in a non-production environment first, especially for Hydra v2 which has significant breaking changes.

## Hydra v1.9.2 → v2.2.0 Breaking Changes

### Major Changes:
1. **Database Schema**: Complete refactoring of internal database structure
2. **Client ID Generation**: OAuth2 Client IDs are now auto-generated (UUIDs)
3. **Client Secret Hashing**: Changed from BCrypt to PBKDF2 for better performance
4. **API Changes**: Several SDK methods and endpoints were renamed/removed
5. **Primary Keys**: All primary keys are now UUIDs

### Migration Steps:
1. **Database Migration**: Run `hydra migrate sql` after deployment
2. **Client Configuration**: Update any hardcoded client IDs to use the new format
3. **API Calls**: Update client applications to use renamed SDK methods
4. **CLI Commands**: Update scripts using Hydra CLI (many commands were renamed)

### Kubernetes-Specific Changes:
- Remove `--dangerous-allow-insecure-redirect-url` and `--dangerous-force-http` flags
- Use `--dev` flag instead for development environments
- Set `hydra.automigration.enabled: true` if using Helm charts

## Kratos v0.10.1 → v1.2.0 Breaking Changes

### Major Changes:
1. **Admin API Path**: Moved from `/` to `/admin` (affects admin endpoints)
2. **OIDC Registration**: Node group for validation failures changed from "oidc" to "default"
3. **Verification Flow**: Changes to "show_verification_ui" behavior
4. **API Stability**: v1.x promises backwards compatibility within the v1 series

### Migration Steps:
1. **Database Migration**: Run `kratos migrate sql` after deployment
2. **Admin API**: Update admin API calls to use `/admin` prefix
3. **OIDC Configuration**: Check OIDC registration flows if using external providers
4. **Verification Flows**: Review verification flow configurations

### Configuration Updates:
- Ensure all required configuration values are set:
  - `selfservice.default_browser_return_url`
  - `identity.schemas`
  - `courier.smtp.connection_uri`

## Updated Manifest Changes

### Hydra Deployment:
- Updated image from `oryd/hydra:v1.9.2-sqlite` to `oryd/hydra:v2.2.0`
- Updated alpine image from `oryd/hydra:v1.9.2-alpine` to `oryd/hydra:v2.2.0-alpine`
- Removed SHA256 pinning (using latest v2.2.0 tags)

### Kratos Deployment:
- Updated image from `oryd/kratos:v0.10.1` to `oryd/kratos:v1.2.0`
- Removed SHA256 pinning (using latest v1.2.0 tag)

## Post-Upgrade Verification

### For Hydra:
1. Verify OAuth2 flows work correctly
2. Check that existing client configurations are accessible
3. Test token generation and validation
4. Verify OIDC endpoints respond correctly

### For Kratos:
1. Test login/registration flows
2. Verify admin API endpoints work with `/admin` prefix
3. Check identity management functionality
4. Test password recovery flows

## Rollback Strategy

If issues occur:
1. **Database**: Restore from backup taken before upgrade
2. **Images**: Revert to previous image versions
3. **Configuration**: Restore previous configuration files

## Support Resources

- [Ory Hydra v2 Migration Guide](https://github.com/ory/hydra/blob/master/UPGRADE.md)
- [Ory Kratos Upgrade Guide](https://www.ory.sh/docs/kratos/guides/upgrade)
- [Ory Community Chat](https://slack.ory.sh/)
- [GitHub Discussions](https://github.com/ory/kratos/discussions)

## Next Steps

1. Review this upgrade plan with your team
2. Schedule maintenance window for database migrations
3. Test upgrade in staging environment
4. Execute production upgrade during planned maintenance
5. Monitor applications for any issues post-upgrade