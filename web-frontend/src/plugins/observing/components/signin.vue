// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<div style="height: 100%">
  <v-tabs dark v-model="active" style="height: 0px;">
    <v-tab key="0"></v-tab>
    <v-tab key="1"></v-tab>
  </v-tabs>
  <v-tabs-items v-model="active">

    <v-tab-item key="0" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-layout row justify-space-around style="margin-top: calc(100vh /2 - 200px);">
          <h4 class="white--text">Sign in to NoctuaSky</h4>
        </v-layout>
        <v-card>
          <v-progress-circular v-if="signInInProgress" indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 60px;"></v-progress-circular>
          <v-card-text>
            <v-text-field :disabled="signInInProgress" name="input-login" label="Email" v-model="email" @input="clearSignInErrorAlert"></v-text-field>
            <v-text-field :disabled="signInInProgress" name="input-pwd" label="Password" v-model="password" type="password" @keyup.native.enter="signIn()" @input="clearSignInErrorAlert"></v-text-field>
            <v-alert error v-model="showSignInErrorAlert">{{ signInErrorAlert }}</v-alert>
            <v-layout row justify-space-around>
              <v-btn @click.stop.native="signIn()" :disabled="signInButtonDisabled || signInInProgress">Sign In</v-btn>
            </v-layout>
          </v-card-text>
          <v-card-actions>
          </v-card-actions>
        </v-card>
        <v-layout row justify-space-around style="margin-top: 30px">
          <p>New to NoctuaSky? <v-btn flat small @click.stop.native="showSignupTab()">Create an account</v-btn></p>
        </v-layout>
      </v-container>
    </v-tab-item>

    <v-tab-item key="1" style="height: 100%">
      <v-container fluid style="height: 100%">
        <v-layout row justify-space-around style="margin-top: calc(100vh /2 - 300px);">
          <h4 class="white--text">Create a NoctuaSky account</h4>
        </v-layout>
        <v-card>
          <v-progress-circular v-if="signInInProgress" indeterminate v-bind:size="50" style="position: absolute; left: 0; right: 0; margin-left: auto; margin-right: auto; margin-top: 60px;"></v-progress-circular>
          <v-card-text>
            <v-layout row justify-space-between>
              <v-text-field name="input-first-name" label="First Name" v-model="signUpFirstName"></v-text-field>
              &nbsp;
              <v-text-field name="input-last-name" label="Last Name" v-model="signUpLastName"></v-text-field>
            </v-layout>
            <form action="">
            <v-text-field required name="input-email" label="Email" autocomplete="email" v-model="signUpEmail" :rules="[rules.email]"></v-text-field>
            <v-text-field required name="input-pwd" label="Create password" autocomplete="new-password" v-model="signUpPassword" type="password" counter :rules="[rules.password]"></v-text-field>
            <v-text-field required name="input-pwd2" label="Confirm password" autocomplete="new-password" v-model="signUpPassword2" type="password" :rules="[rules.confirmPassword]"></v-text-field>
            </form>
            <v-alert error v-model="showSignUpErrorAlert">{{ signUpErrorAlert }}</v-alert>
            <v-layout row justify-space-around>
              <v-btn @click.stop.native="signUp()" :disabled="formValid">Sign Up</v-btn>
            </v-layout>
          </v-card-text>
        </v-card>
        <v-layout row justify-space-around style="margin-top: 30px">
          <p><v-btn flat small @click.stop.native="showSigninTab()">Back to SignIn</v-btn></p>
        </v-layout>
      </v-container>
    </v-tab-item>

  </v-tabs-items>
</div>
</template>

<script>

export default {
  data: function () {
    return {
      email: '',
      password: '',
      signInErrorAlert: '',
      signUpFirstName: '',
      signUpLastName: '',
      signUpPassword: '',
      signUpPassword2: '',
      signUpEmail: '',
      signUpErrorAlert: '',
      active: '0',

      rules: {
        email: (value) => {
          if (value === '') {
            return true
          }
          const pattern = /^(([^<>()[\]\\.,;:\s@"]+(\.[^<>()[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/
          return pattern.test(value) || 'Invalid e-mail.'
        },
        password: (value) => {
          if (value === '') {
            return true
          }
          if (value.length < 8) {
            return 'Use at least 8 characters'
          }
          return true
        },
        confirmPassword: (value) => {
          if (value === '') {
            return true
          }
          if (value !== this.signUpPassword) {
            return 'Passwords don\'t match'
          }
          return true
        }
      }
    }
  },
  mounted: function () {
    this.$store.dispatch('observing/initLoginStatus')
  },
  methods: {
    signIn: function () {
      var that = this
      this.$store.dispatch('observing/signIn', {'email': this.email, 'password': this.password}).catch(function (res) {
        if (res.message) {
          that.signInErrorAlert = res.message
        } else if (res.errors) {
          that.signInErrorAlert = ''
          for (let k in res.errors) {
            that.signInErrorAlert += k + ': ' + res.errors[k] + '\n'
          }
        }
      })
    },
    signUp: function () {
      var that = this
      this.$store.dispatch('observing/signUp', {'password': this.signUpPassword, 'email': this.signUpEmail, 'firstName': this.signUpFirstName, 'lastName': this.signUpLastName}).then(res => {
        that.showSigninTab()
      }).catch(function (res) {
        if (res.message) {
          that.signUpErrorAlert = res.message
        } else if (res.errors) {
          that.signUpErrorAlert = ''
          for (let k in res.errors) {
            that.signUpErrorAlert += k + ': ' + res.errors[k] + '\n'
          }
        }
      })
    },
    showSignupTab: function () {
      this.active = '1'
    },
    showSigninTab: function () {
      this.active = '0'
    },
    clearSignInErrorAlert: function () {
      this.signInErrorAlert = ''
    },
    clearSignUpErrorAlert: function () {
      this.signUpErrorAlert = ''
    }
  },
  computed: {
    formValid: function () {
      return this.signUpUserName !== '' && this.signUpPassword.len > 5 && this.signUpPassword === this.signUpPassword2
    },
    signInButtonDisabled: function () {
      return this.userName === '' || this.password === ''
    },
    signInInProgress: function () {
      return this.$store.state.plugins.observing.noctuaSky.loginStatus === 'signInInProgress'
    },
    showSignInErrorAlert: function () {
      return this.signInErrorAlert !== ''
    },
    showSignUpErrorAlert: function () {
      return this.signUpErrorAlert !== ''
    }
  }
}
</script>

<style>
.tabs {
  height: 100%;
}
.tabs__items {
  height: 100%;
  border-style: none;
}
.tabs__bar {
  background-color: #303030;
}
</style>
