namespace Admin {
    static bool admin_higher( client_t *admin, client_t *victim )
    {
        // console always wins
        if( !admin )
            return true;

        if ( victim->has_permission(ADMF_IMMUTABLE) )
            return false;

        return ( admin->admin->level <= victim->admin->level );
    }
}
